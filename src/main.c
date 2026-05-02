#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>


volatile sig_atomic_t g_terminated = 0;

static void sig_handler(int sig) {
    (void)sig;
    g_terminated = 1;
}

typedef struct {
  int mode; /* 1 = server, 2 = client */
  const char *addr;
  int port;
  const char *input;
  const char *output;
  int timeout;
} cfg;

static void usage(void){
  printf("Usage:\n");
  // server
  printf("Server:");
  printf("./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT] [-h | --help]\n");
  // client
  printf("Client:");
  printf("./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT] [-h | --help]\n");

}

static void print_config(const cfg *cfg){
    printf("Config:\n");

    printf("  mode    = ");
    if (cfg->mode == 1) {
        printf("server\n");
    } else if (cfg->mode == 2) {
        printf("client\n");
    } else {
        printf("unknown\n");
    }

    printf("  addr    = %s\n", cfg->addr ? cfg->addr : "(null)");
    printf("  port    = %d\n", cfg->port);
    printf("  input   = %s\n", cfg->input ? cfg->input : "(stdin)");
    printf("  output  = %s\n", cfg->output ? cfg->output : "(stdout)");
    printf("  timeout = %d\n", cfg->timeout);
}

static int parse_args(int argc, char **argv, cfg *cfg){
  int opt;
  while((opt = getopt(argc, argv, "hscp:a:i:o:w:")) != -1){
    switch(opt) {
      // modes
      case 's':
        if(cfg->mode == 2){ fprintf(stderr, "Cannot use -s and -c together\n"); return 1; }
        cfg->mode = 1;
        break;
      case 'c':
        if(cfg->mode == 1){ fprintf(stderr, "Cannot use -s and -c together\n"); return 1; }
        cfg->mode = 2;
        break;
      // port
      case 'p': {
        char *end;
        long v = strtol(optarg, &end, 10);
        if (*end || v < 1 || v > 65535) { fprintf(stderr, "Invalid port\n"); return 1; }
        cfg->port = (int)v;
        break;
      }
      case 'a':
        cfg->addr = optarg;
        break;
      case 'i':
        cfg->input = optarg;
        break;
      case 'o':
        cfg->output = optarg;
        break;

      case 'w': {
        char *end;
        long v = strtol(optarg, &end, 10);
        if(*end || v < 1){ fprintf(stderr, "Invalid timeout %s \n", optarg); return 1; }
        cfg->timeout = v;
        break;
      }

      case 'h':
        usage();
        return 1;
      default:
        fprintf(stderr, "-h for help\n");
        return 1;
    }
  }

  if (cfg->mode == 0){ 
    fprintf(stderr, "Error: specify -s (server) or -c (client)\n"); 
    return 1;
  }
  if(cfg->port < 0){
    fprintf(stderr, "Error: -p PORT is required\n"); 
    return 1;
  }

  return 0;
}


int main(int argc, char *argv[]){
  /* Signal handling */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT,  &sa, NULL);

  if (argc == 1) { usage(); return 0; }

  cfg main_config = {
    .mode = 0,
    .addr = NULL,
    .port = -1,
    .input = NULL,
    .output = NULL,
    .timeout = 1,
  };

  int ret = parse_args(argc, argv, &main_config);
  if(ret != 0) return ret;

  print_config(&main_config);

  return 0;
}


