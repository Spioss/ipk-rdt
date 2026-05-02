#include "server.h"

#include <stdio.h>

int run_server(const server_config *cfg){
  fprintf(stderr, "Server started\n");
  fprintf(stderr, "  bind_addr = %s\n", cfg->bind_addr ? cfg->bind_addr : "(all)");
  fprintf(stderr, "  port      = %u\n", cfg->port);
  fprintf(stderr, "  output_fd = %d\n", cfg->output_fd);
  fprintf(stderr, "  timeout_s = %d\n", cfg->timeout_s);

  return 0;
}