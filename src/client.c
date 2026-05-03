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


int run_client(const client_config *cfg){
  fprintf(stderr, "Client started\n");
  fprintf(stderr, "  host      = %s\n", cfg->host);
  fprintf(stderr, "  port      = %u\n", cfg->port);
  fprintf(stderr, "  input_fd  = %d\n", cfg->input_fd);
  fprintf(stderr, "  timeout_s = %d\n", cfg->timeout_s);

  return 0;
}