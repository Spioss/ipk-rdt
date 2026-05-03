#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>

#include "server.h"
#include "netUtils.h"
#include "protocol.h"

extern volatile sig_atomic_t g_terminated;

// build and send a control packet
static void send_controlPkt(int fd, uint8_t type, uint32_t conn_id, uint32_t seq, uint32_t ack, const addr_t *dst){
  pkt p;
  memset(&p, 0, sizeof(p));
  p.hdr.magic = MAGIC;
  p.hdr.type = type;
  p.hdr.conn_id = conn_id;
  p.hdr.seq = seq;
  p.hdr.ack = ack;
  p.hdr.data_len = 0;
  net_send(fd, &p, dst);
}

// clamp select timeout to nearest retransmit deadline 
static void clamp_tv(struct timeval *tv, long rto, const struct timespec *ts){
  long remaining = rto - elapsed_ms(ts);
  if (remaining < 1) remaining = 1;
  long tv_ms = tv->tv_sec * 1000 + tv->tv_usec / 1000;
  if (remaining < tv_ms) {
    tv->tv_sec  = remaining / 1000;
    tv->tv_usec = (remaining % 1000) * 1000;
  }
}

// state: SS_LISTEN
static void handle_listen(int fd, server_ctx *ctx, const pkt *pkt, const addr_t *src){
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
    send_ctrl(fd, PKT_SYNACK, ctx->conn_id, ctx->server_initial, ctx->client_initial + 1, &ctx->client_addr);
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
  //TO DO
}











int run_server(const server_config *cfg){
  int fd_serverSocket = create_server(cfg->bind_addr, cfg->port);
  if (fd_serverSocket < 0) return 1;

  fprintf(stderr, "Server listening on port %u\n", cfg->port);


  // will need to reado this initialize context then process pkt and retransmissions
  // Initialize context 
  server_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.state      = SS_LISTENING;
  ctx.output_fd  = cfg->output_fd;
  ctx.timeout_s  = cfg->timeout_s;
  ctx.synack_rto = RTO_INITIAL_MS;
  ctx.finack_rto = RTO_INITIAL_MS;
  getrandom(&ctx.server_initial, sizeof(ctx.server_initial), 0);
  now_ts(&ctx.progress_ts);
}