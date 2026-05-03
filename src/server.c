#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>

#include "server.h"
#include "netUtils.h"
#include "protocol.h"

extern volatile sig_atomic_t g_terminated;


// state: SS_LISTEN
static void handle_listen(int fd, server_ctx *ctx, const pkt *pkt, const addr *src){
  if (pkt->hdr.type != PKT_SYN) return;

  ctx->conn_id = pkt->hdr.conn_id;
  ctx->client_initial = pkt->hdr.seq;
  ctx->client_addr = *src;

  send_controlPkt(fd, PKT_SYNACK, ctx->conn_id, ctx->server_initial, ctx->client_initial + 1, &ctx->client_addr);
  now_ts(&ctx->synack_ts); // when we send syn-ack
  ctx->synack_pending = true;
  ctx->state = SS_SYN_RECEIVED;
  fprintf(stderr, "SYN received, conn_id=0x%08X\n", ctx->conn_id);
}

// state: SS_SYN_RECIEVED
static void handle_syn_received(int fd, server_ctx *ctx, const pkt *pkt){
  if (pkt->hdr.type == PKT_SYN) {
    send_controlPkt(fd, PKT_SYNACK, ctx->conn_id, ctx->server_initial, ctx->client_initial + 1, &ctx->client_addr);
    now_ts(&ctx->synack_ts); // reset timer
    return;
  }

  if (pkt->hdr.type != PKT_ACK) return;
  if (pkt->hdr.ack != ctx->server_initial + 1) return;

  ctx->synack_pending = false;
  ctx->state = SS_TRANSFERRING;
  now_ts(&ctx->progress_ts);
  fprintf(stderr, "Connection established (conn_id=0x%08X)\n", ctx->conn_id);
}

// state: SS_TRANSFERING - DATA PACKETS
static int handle_data(int fd, server_ctx *ctx, const pkt *pkt){
  uint32_t seq = pkt->hdr.seq; // byte so if seq=0 and data_len=500 then next byte seq=500
  uint16_t data_len = pkt->hdr.data_len;

  // ACK was lost need to send again 
  if (seq < ctx->rcv_expected){
    send_controlPkt(fd, PKT_ACK, ctx->conn_id, 0, ctx->rcv_expected, &ctx->client_addr);
    return 0;
  }

  // if outside window drop 
  uint32_t win_end = ctx->rcv_expected + (uint32_t)(WINDOW_SIZE * MAX_PAYLOAD);
  if (seq >= win_end) return 0;
  // (client will send again when we move window)

  // store into recieve buf (if slot is occupied we ignore)
  int idx = (int)((seq / MAX_PAYLOAD) % WINDOW_SIZE);
  if(!ctx->rcv_buf[idx].occupied){
    memcpy(ctx->rcv_buf[idx].data, pkt->data, data_len);
    ctx->rcv_buf[idx].len = data_len;
    ctx->rcv_buf[idx].occupied = true;
  }

  bool progressed = (seq == ctx->rcv_expected);
  while(true){
    int current_idx = (int)((ctx->rcv_expected / MAX_PAYLOAD) % WINDOW_SIZE);
    if(!ctx->rcv_buf[current_idx].occupied) break;

    uint16_t n = ctx->rcv_buf[current_idx].len;
    uint8_t *buf = ctx->rcv_buf[current_idx].data;

    // write loop
    ssize_t written = 0;
    while(written < n){
      ssize_t w = write(ctx->output_fd, buf + written, n - written);
      if(w < 0){
        fprintf(stderr, "write output: %s\n", strerror(errno));
        return -1;
      }
      written += w;
    }
    ctx->rcv_buf[current_idx].occupied = false;
    ctx->rcv_expected += n;
  }

  // final ACK
  send_controlPkt(fd, PKT_ACK, ctx->conn_id, 0, ctx->rcv_expected, &ctx->client_addr);
  if(progressed) now_ts(&ctx->progress_ts);

  return 0;
}


// state: SS_TRANSFERRING - FIN PACKET
static int handle_fin(int fd, server_ctx *ctx, const pkt *pkt){
  // fin seq if 5000 bytes then fin_seq = 5000 
  ctx->fin_seq = pkt->hdr.seq;

  // still some data missing, ACK and wait
  if(ctx->rcv_expected != ctx->fin_seq){
    send_controlPkt(fd, PKT_ACK, ctx->conn_id, 0, ctx->rcv_expected, &ctx->client_addr);
    return 0;
  }

  // All data recieved
  send_controlPkt(fd, PKT_FINACK, ctx->conn_id, ctx->server_initial + 1, ctx->fin_seq + 1, &ctx->client_addr);
  now_ts(&ctx->finack_ts);
  ctx->finack_sent = true;
  ctx->finack_rto = 200;
  ctx->state = SS_FIN_RECEIVED;
  now_ts(&ctx->progress_ts);
  fprintf(stderr, "FIN recieved, sending FIN-ACK\n");
  return 0;
}

// state: SS_TRANSFERRING
static int handle_transferring(int fd, server_ctx *ctx, const pkt *pkt){
  if(pkt->hdr.type == PKT_DATA) return handle_data(fd, ctx, pkt);

  if(pkt->hdr.type == PKT_FIN){
    handle_fin(fd, ctx, pkt);
    return 0;
  }

  // ACK and others ignore in transfer
  return 0;
}


// state: SS_FIN_RECEIVED
static int handle_fin_recieved(int fd, server_ctx *ctx, const pkt *pkt){
  if(pkt->hdr.type == PKT_FIN){
    send_controlPkt(fd, PKT_FINACK, ctx->conn_id, ctx->server_initial + 1, ctx->fin_seq + 1, &ctx->client_addr);
    now_ts(&ctx->finack_ts);
    ctx->finack_rto = 200;
    return 0;
  }

  if(pkt->hdr.type == PKT_ACK){
    // should be final ack - transfer complete
    ctx->finack_sent = false;
    ctx->state = SS_DONE;
  }
  return 0;
}


// main dispatcher process one packet
static int process_packet(int fd, server_ctx *ctx){
  pkt recieved_pkt;
  addr src;

  // packet from socket
  if(!recieve_pkt(fd, &recieved_pkt, &src)) return 0;

  // listen should accept packets from anyone
  if(ctx->state == SS_LISTENING){
    handle_listen(fd, ctx, &recieved_pkt, &src);
    return 0;
  }

  // ignore packets from others
  if(!addr_equal(&src, &ctx->client_addr)) return 0;
  if(recieved_pkt.hdr.conn_id != ctx->conn_id) return 0;

  // if(ctx->state == SS_SYN_RECEIVED){
  //   handle_syn_received(fd, ctx, &recieved_pkt);
  //   return 0;
  // }

  // if(ctx->state == SS_TRANSFERRING){
  //   handle_transferring(fd, ctx, &recieved_pkt);
  //   return 0;
  // }

  // if(ctx->state == SS_FIN_RECEIVED){
  //   handle_fin_recieved(fd, ctx, &recieved_pkt);
  //   return 0;
  // }

  switch(ctx->state){
    case SS_SYN_RECEIVED:
      handle_syn_received(fd, ctx, &recieved_pkt);
      return 0;
    case SS_TRANSFERRING:
      handle_transferring(fd, ctx, &recieved_pkt);
      return 0;
    case SS_FIN_RECEIVED:
      handle_fin_recieved(fd, ctx, &recieved_pkt);
      return 0;
  
    default:
      break;
  }

  return 0;
}


// retransmitions logic
static int handle_retransmits(int fd, server_ctx *ctx){
  if(ctx->synack_pending){
    if(elapsed_ms(&ctx->synack_ts) >= ctx->synack_rto){

      if (elapsed_ms(&ctx->progress_ts) >= (long)ctx->timeout_s * 1000) {
        fprintf(stderr, "Timeout: no ACK to SYN-ACK\n");
        return -1;
      }

      // send SYN-ACK and double the timeout
      send_controlPkt(fd, PKT_SYNACK, ctx->conn_id, ctx->server_initial, ctx->client_initial + 1, &ctx->client_addr);
      ctx->synack_rto *= 2;
      if (ctx->synack_rto > RTO_MAX_MS) ctx->synack_rto = RTO_MAX_MS;
      now_ts(&ctx->synack_ts);
    }
  }
  
  if(ctx->finack_sent){
    if(elapsed_ms(&ctx->finack_ts) >= ctx->finack_rto){
      // final ack lost
      if(elapsed_ms(&ctx->progress_ts) >= (long)ctx->timeout_s * 1000){
          fprintf(stderr, "Transfer complete (final ACK lost)\n");
          return 1; // success via timeout -> run_server returns exit 0
      }
      
      // retransmit FINACK
      send_controlPkt(fd, PKT_FINACK, ctx->conn_id, ctx->server_initial + 1, ctx->fin_seq + 1, &ctx->client_addr);
      ctx->finack_rto *= 2;
      if(ctx->finack_rto > RTO_MAX_MS) ctx->finack_rto = RTO_MAX_MS;
      now_ts(&ctx->finack_ts);
    }
  }

  return 0;
}

 

// just check if global timeout was fullfilled or not
static int check_progress_timeout(server_ctx *ctx){
  if (ctx->state == SS_DONE) return 0;

  // in ms!!!!!!!!
  if (elapsed_ms(&ctx->progress_ts) >= (long)ctx->timeout_s * 1000) {
    fprintf(stderr, "Timeout: no progress\n");
    return -1;
  }
  
  return 0;
}


int run_server(const server_config *cfg){
  int fd_serverSocket = create_server(cfg->bind_addr, cfg->port);
  if (fd_serverSocket < 0) return 1;

  fprintf(stderr, "Server listening on port %u\n", cfg->port);


  // will need to reado this initialize context then process pkt and retransmissions
  // Initialize context 
  server_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.state = SS_LISTENING;
  ctx.output_fd = cfg->output_fd;
  ctx.timeout_s = cfg->timeout_s;
  ctx.synack_rto = RTO_INITIAL_MS;
  ctx.finack_rto = RTO_INITIAL_MS;
  srand((unsigned)time(NULL));
  ctx.server_initial = (uint32_t)rand();
  now_ts(&ctx.progress_ts);

  while(!g_terminated){
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000; // 50ms default
    
    if(ctx.synack_pending) clamp_tv(&tv, ctx.synack_rto, &ctx.synack_ts);
    if(ctx.finack_sent) clamp_tv(&tv, ctx.finack_rto, &ctx.finack_ts);

    // waiting for incoming packet
    fd_set rfds; // read file descriptor set
    FD_ZERO(&rfds);
    FD_SET(fd_serverSocket, &rfds);
    int sel = select(fd_serverSocket + 1, &rfds, NULL, NULL, &tv);
    if(sel < 0){
      if(errno == EINTR) continue;
      fprintf(stderr, "select: %s\n", strerror(errno));
      close(fd_serverSocket);
      return 1;
    }

    // process packet
    if(sel > 0 && FD_ISSET(fd_serverSocket, &rfds)){
      if(process_packet(fd_serverSocket, &ctx) < 0){
        close(fd_serverSocket);
        return 1;
      }
    }

    // retrnasmissions
    int retran = handle_retransmits(fd_serverSocket, &ctx);
    if(retran < 0) { close(fd_serverSocket); return 1; }
    if(retran > 0) { close(fd_serverSocket); return 0; }

    // check for gloabla timeout 
    if(check_progress_timeout(&ctx) < 0){
      close(fd_serverSocket);
      return 1;
    }

    // check done 
    if (ctx.state == SS_DONE){
      fprintf(stderr, "Transfer complete\n");
      close(fd_serverSocket);
      return 0;
    }
  } 

  // terminated by signal
  close(fd_serverSocket);
  return 1;
}