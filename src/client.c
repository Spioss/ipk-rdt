#include "client.h"

#include <stdio.h>

int run_client(const client_config *cfg){
  fprintf(stderr, "Client started\n");
  fprintf(stderr, "  host      = %s\n", cfg->host);
  fprintf(stderr, "  port      = %u\n", cfg->port);
  fprintf(stderr, "  input_fd  = %d\n", cfg->input_fd);
  fprintf(stderr, "  timeout_s = %d\n", cfg->timeout_s);

  return 0;
}