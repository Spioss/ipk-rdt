#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/random.h>
#include <math.h>

#include "client.h"
#include "protocol.h"
#include "netUtils.h"

extern volatile sig_atomic_t g_terminated;

// RTT functions
static void rtt_init(rtt *r){
  r->srtt = 0.0; // average
  r->rttvar = 0.0; // variance
  r->rto_ms = RTO_INITIAL_MS; // 200 ms default
  r->seeded = false;
}

// sample in ms
static void rtt_update(rtt *r, long sample){
  const double beta = 0.25;
  const double alpha= 0.125;

  if(!r->seeded){ // first sample
    r->srtt = (double) sample;
    r->rttvar = (double) sample / 2.0;
    r->seeded = true;
  } else { // Jacobson Algorithm RFC 6298
    double diff = fabs(r->srtt - (double)sample); // abosolute value
    r->rttvar = (1.0 - beta) * r->rttvar + beta * diff;
    r->srtt = (1.0 - alpha) * r->srtt + alpha * (double)sample;
  }
  r->rto_ms = (long)(r->srtt + 4.0 * r->rttvar);
  if (r->rto_ms < RTO_MIN_MS) r->rto_ms = RTO_MIN_MS;
  if (r->rto_ms > RTO_MAX_MS) r->rto_ms = RTO_MAX_MS;
}

static inline int slot_idx(uint32_t seq){
  return (int)((seq / MAX_PAYLOAD) % WINDOW_SIZE);
}

// state: CS_CONNECTING (SYN-ACK from server)
static int handle_connecting(int fd, client_ctx *ctx, const pkt *pkt){
  if(pkt->hdr.type != PKT_SYNACK) return 0;
  if(pkt->hdr.ack != ctx->client_initial + 1) return 0;

  // server isn
  ctx->server_initial = pkt->hdr.seq;

  //rtt
  long sample = elapsed_ms(&ctx->syn_sent_ts);
  rtt_update(&ctx->rtt, sample);

  //ACK
  send_controlPkt(fd, PKT_ACK, ctx->conn_id, ctx->client_initial + 1, ctx->server_initial + 1, &ctx->srv_addr);
  ctx->state = CS_TRANSFERRING;
  now_ts(&ctx->progress_ts);
  fprintf(stderr, "Connection established (conn_id=0x%08X)\n", ctx->conn_id);
  return 0;
}

// state: CS_TRANSFERING (recieved ACK from server)
static int handle_ack(int fd, client_ctx *ctx, const pkt *pkt){
  (void)fd; // uh oh

  if(pkt->hdr.type != PKT_ACK) return 0;

  // ignore old or dup ACK
  if(pkt->hdr.ack <= ctx->send_base) return 0;

  for(int i = 0; i < WINDOW_SIZE; i++){
    if(!ctx->window[i].in_use) continue;
    
    // seq + len <= ack
    if(ctx->window[i].seq + ctx->window[i].len <= pkt->hdr.ack){
      // Karn's algorithm - not count retransmitted packets
      if(ctx->window[i].retransmits == 0){
        long sample = elapsed_ms(&ctx->window[i].sent_at);
        rtt_update(&ctx->rtt, sample);
      }
      ctx->window[i].in_use = false;
    }
  }

  bool progressed = (pkt->hdr.ack > ctx->send_base);
  ctx->send_base = pkt->hdr.ack;

  if(progressed) now_ts(&ctx->progress_ts);
  return 0;
}

//state: CS_FIN_WAIT - recieved FIN-ACK from server
static int handle_finack(int fd, client_ctx *ctx, const pkt *pkt){
  if(pkt->hdr.type != PKT_FINACK) return 0;
  
  // final ACK
  send_controlPkt(fd, PKT_ACK, ctx->conn_id, 0, pkt->hdr.seq + 1, &ctx->srv_addr);

  // wait to ensure the ACK is sent before termination 
  struct timespec ts = {0, 50000000L}; /* 50ms */
  nanosleep(&ts, NULL);

  ctx->state = CS_DONE;
  fprintf(stderr, "Transfer complete\n");
  return 0;
}

// main dispatch 
static int process_packet(int fd, client_ctx *ctx){
  pkt recieved_pkt;
  addr src;

  if(!recieve_pkt(fd, &recieved_pkt, &src)) return 0;

  if(recieved_pkt.hdr.conn_id != ctx->conn_id) return 0;

  // if(ctx->state == CS_CONNECTING){
  //   return handle_connecting(fd, ctx, &recieved_pkt);
  // }
  // if(ctx->state == CS_TRANSFERRING)


  switch(ctx->state){
    case CS_CONNECTING:
      return handle_connecting(fd, ctx, &recieved_pkt);
    case CS_TRANSFERRING:
      return handle_ack(fd, ctx, &recieved_pkt);
    case CS_FIN_WAIT:
      return handle_finack(fd, ctx, &recieved_pkt);

    default:
      break;
  }

  return 0;
}


// reads the input a sends data packets to window
static int fill_window(int fd, client_ctx *ctx){
  while(!ctx->eof){
    if(ctx->send_next >= ctx->send_base + (uint32_t)(WINDOW_SIZE * MAX_PAYLOAD))
      break; // full window

    int idx = slot_idx(ctx->send_next);
    if (ctx->window[idx].in_use) break; // slot in use;

    ssize_t n = read(ctx->input_fd, ctx->window[idx].data, MAX_PAYLOAD);
    if (n < 0){
      fprintf(stderr, "read input: %s\n", strerror(errno));
      return -1;
    }
    if(n == 0){
      ctx->eof = true; // end of input
      break;
    }

    //fill slot (save a copy of the data for retransmission)
    ctx->window[idx].seq = ctx->send_next;
    ctx->window[idx].len = (uint16_t)n;
    ctx->window[idx].rto_ms = ctx->rtt.rto_ms;
    ctx->window[idx].retransmits = 0;
    ctx->window[idx].in_use = true;
    now_ts(&ctx->window[idx].sent_at);
    
    // send DATA paket
    pkt data;
    memset(&data, 0, sizeof(data));
    data.hdr.magic = MAGIC;
    data.hdr.type = PKT_DATA;
    data.hdr.conn_id = ctx->conn_id;
    data.hdr.seq = ctx->send_next;
    data.hdr.ack = 0;
    data.hdr.data_len = (uint16_t)n;
    memcpy(data.data, ctx->window[idx].data, n);
    send_pkt(fd, &data, &ctx->srv_addr);

    ctx->send_next += (uint32_t)n; // next
    ctx->total_read += (uint32_t)n; // total read bytes
  }

  return 0;
}

// retransmits all slots that have exceeded their RTO
static int retransmit_window(int fd, client_ctx *ctx){
  for (int i = 0; i < WINDOW_SIZE; i++){
    if(!ctx->window[i].in_use) continue;

    // timeout for 1 slot ?
    if(elapsed_ms(&ctx->window[i].sent_at) < ctx->window[i].rto_ms)
      continue;

    // retransmit
    pkt pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.magic = MAGIC;
    pkt.hdr.type = PKT_DATA;
    pkt.hdr.conn_id = ctx->conn_id;
    pkt.hdr.seq = ctx->window[i].seq;
    pkt.hdr.ack = 0;
    pkt.hdr.data_len = ctx->window[i].len;
    memcpy(pkt.data, ctx->window[i].data, ctx->window[i].len);
    send_pkt(fd, &pkt, &ctx->srv_addr);

    // backoff
    ctx->window[i].retransmits++;
    ctx->window[i].rto_ms *= 2;
    if (ctx->window[i].rto_ms > RTO_MAX_MS) ctx->window[i].rto_ms = RTO_MAX_MS;
    now_ts(&ctx->window[i].sent_at);

    fprintf(stderr, "Retransmit seq=%u (attempt %d)\n", ctx->window[i].seq, ctx->window[i].retransmits);
  }
  return 0;
}

// retrans fin if timeout
static int retransmit_fin(int fd, client_ctx *ctx){
  send_slot *first = &ctx->window[0];

  if(!first->in_use) return 0;

  if(elapsed_ms(&first->sent_at) < first->rto_ms) return 0;

  // gloabal timeout
  if (elapsed_ms(&ctx->progress_ts) >= (long)ctx->timeout_s * 1000) {
    fprintf(stderr, "Timeout waiting for FIN-ACK\n");
    return -1;
  }

  // retransmit FIN
  pkt pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.hdr.magic = MAGIC;
  pkt.hdr.type = PKT_FIN;
  pkt.hdr.conn_id  = ctx->conn_id;
  pkt.hdr.seq = first->seq;  // total bytes sent
  pkt.hdr.ack = 0;
  pkt.hdr.data_len = 0;
  send_pkt(fd, &pkt, &ctx->srv_addr);

  // backoff
  first->retransmits++;
  first->rto_ms *= 2;
  if(first->rto_ms > RTO_MAX_MS) first->rto_ms = RTO_MAX_MS;
  now_ts(&first->sent_at);

  fprintf(stderr, "Retransmit FIN seq=%u (attempt %d)\n", first->seq, first->retransmits);
  return 0;
}

// compute shortest timeout for select()
static struct timeval compute_timeout(client_ctx *ctx){
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 10000; // 10ms default

  if(ctx->state == CS_CONNECTING){
    clamp_tv(&tv, ctx->syn_rto, &ctx->syn_sent_ts);
  } else if (ctx->state == CS_TRANSFERRING){
    for(int i = 0; i < WINDOW_SIZE; i++){
      if(!ctx->window[i].in_use) continue;
      clamp_tv(&tv, ctx->window[i].rto_ms, &ctx->window[i].sent_at);
    }
  } else if (ctx->state == CS_FIN_WAIT){
    send_slot *first = &ctx->window[0];
    if(first->in_use) clamp_tv(&tv, first->rto_ms, &first->sent_at);
  }

  return tv;
}


int run_client(const client_config *cfg){
  addr srv_addr;
  int fd_client = create_client(cfg->host, cfg->port, &srv_addr);
  if (fd_client < 0) return 1;

  // context init
  client_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.state = CS_CONNECTING;
  ctx.input_fd = cfg->input_fd;
  ctx.srv_addr = srv_addr;
  ctx.syn_rto = RTO_INITIAL_MS;
  getrandom(&ctx.conn_id, sizeof(ctx.conn_id), 0);
  getrandom(&ctx.client_initial, sizeof(ctx.client_initial), 0);
  rtt_init(&ctx.rtt);
  now_ts(&ctx.progress_ts);

  // first SYN
  send_controlPkt(fd_client, PKT_SYN, ctx.conn_id, ctx.client_initial, 0, &ctx.srv_addr);
  now_ts(&ctx.syn_sent_ts);
  fprintf(stderr, "SYN sent (conn_id=0x%08X)\n", ctx.conn_id);

  while(!g_terminated){
    struct timeval tv = compute_timeout(&ctx); // timeout for select()

    //wait for packet
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_client, &rfds);
    int sel = select(fd_client + 1, &rfds, NULL, NULL, &tv);
    if (sel < 0){
      if(errno == EINTR) continue;
      fprintf(stderr, "select: %s\n", strerror(errno));
      close(fd_client);
      return 1;
    }

    // process packet
    if(sel > 0 && FD_ISSET(fd_client, &rfds)){
      if(process_packet(fd_client, &ctx) < 0){
        close(fd_client);
        return 1;
      }
    }

    // state specific actions
    if(ctx.state == CS_CONNECTING){
      // retransmit syn if timeout
      if(elapsed_ms(&ctx.syn_sent_ts) >= ctx.syn_rto){
        if(elapsed_ms(&ctx.progress_ts) >= (long)cfg->timeout_s * 1000){
          fprintf(stderr, "Timeout: no response to SYN\n");
          close(fd_client);
          return 1;
        }
        // backoff
        ctx.syn_rto *= 2;
        if(ctx.syn_rto > RTO_MAX_MS) ctx.syn_rto = RTO_MAX_MS;
        
        send_controlPkt(fd_client, PKT_SYN, ctx.conn_id, ctx.client_initial, 0, &ctx.srv_addr);
        now_ts(&ctx.syn_sent_ts);
        fprintf(stderr, "Retransmit SYN\n");

      }
    }

    if(ctx.state == CS_TRANSFERRING){
      if(fill_window(fd_client, &ctx) < 0){
        close(fd_client);
        return 1;
      }

      retransmit_window(fd_client, &ctx);

      // global timeout
      if (elapsed_ms(&ctx.progress_ts) >= (long)cfg->timeout_s * 1000) {
        fprintf(stderr, "Timeout: no protocol progress\n");
        close(fd_client);
        return 1;
      }

      // everything sent | sent FIN
      if (ctx.eof && ctx.send_base == ctx.send_next) {
        // init fin
        memset(&ctx.window[0], 0, sizeof(ctx.window[0]));
        ctx.window[0].seq = ctx.send_next; /* total bytes */
        ctx.window[0].len = 0;
        ctx.window[0].rto_ms = ctx.rtt.rto_ms;
        ctx.window[0].retransmits = 0;
        ctx.window[0].in_use = true;
        now_ts(&ctx.window[0].sent_at);

        // send
        pkt fp;
        memset(&fp, 0, sizeof(fp));
        fp.hdr.magic = MAGIC;
        fp.hdr.type = PKT_FIN;
        fp.hdr.conn_id = ctx.conn_id;
        fp.hdr.seq = ctx.send_next;
        fp.hdr.ack = 0;
        fp.hdr.data_len = 0;
        send_pkt(fd_client, &fp, &ctx.srv_addr);

        ctx.state = CS_FIN_WAIT;
        now_ts(&ctx.progress_ts);
        fprintf(stderr, "FIN sent (seq=%u)\n", ctx.send_next);
      }
    }

    if(ctx.state == CS_FIN_WAIT){
      if(retransmit_fin(fd_client, &ctx) < 0){
        close(fd_client);
        return 1;
      }
    }

    if(ctx.state == CS_DONE){
      close(fd_client);
      return 0;
    }

  }

  // terminated by signal
  close(fd_client);
  return 1;
}