#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "client.h"
#include "config.h"
#include "server.h"


static void sig_handler(int sig) {
  (void)sig;
  g_terminated = 1;
}

static void usage(void) {
  printf("Usage:\n");
  // server
  printf("Server:");
  printf(
      "./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT] [-h | "
      "--help]\n");
  // client
  printf("Client:");
  printf(
      "./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT] [-h | --help]\n");
}

__attribute__((unused))static void print_config(const cfg* cfg) {
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

static int parse_args(int argc, char** argv, cfg* cfg) {

  static struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "hscp:a:i:o:w:", long_opts, NULL)) != -1) {
    switch (opt) {
      // modes
      case 's':
        if (cfg->mode == 2) {
          fprintf(stderr, "Cannot use -s and -c together\n");
          return 1;
        }
        cfg->mode = 1;
        break;
      case 'c':
        if (cfg->mode == 1) {
          fprintf(stderr, "Cannot use -s and -c together\n");
          return 1;
        }
        cfg->mode = 2;
        break;
      // port
      case 'p': {
        char* end;
        long v = strtol(optarg, &end, 10);
        if (*end || v < 1 || v > 65535) {
          fprintf(stderr, "Invalid port\n");
          return 1;
        }
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
        char* end;
        long v = strtol(optarg, &end, 10);
        if (*end || v < 1) {
          fprintf(stderr, "Invalid timeout %s \n", optarg);
          return 1;
        }
        cfg->timeout = (int)v;
        break;
      }

      case 'h':
        usage();
        return 2;
      default:
        fprintf(stderr, "-h for help\n");
        return 1;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
    return 1;
  }

  if (cfg->mode == 0) {
    fprintf(stderr, "Error: specify -s (server) or -c (client)\n");
    return 1;
  }
  if (cfg->port < 0) {
    fprintf(stderr, "Error: -p PORT is required\n");
    return 1;
  }

  if (cfg->mode == 2 && cfg->addr == NULL) {
    fprintf(stderr, "Error: -a HOST is required for client mode\n");
    return 1;
  }

  return 0;
}

int main(int argc, char* argv[]) {
  /* Signal handling */
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  if (argc == 1) {
    usage();
    return 1;
  }

  cfg main_config = {
      .mode = 0,
      .addr = NULL,
      .port = -1,
      .input = NULL,
      .output = NULL,
      .timeout = 1,
  };

  int ret = parse_args(argc, argv, &main_config);
  if (ret == 2) return 0; // -h should be 0
  if (ret != 0) return ret;

  // --- SERVER ---
  if (main_config.mode == 1) {
    int out_fd = STDOUT_FILENO;
    if (main_config.output && strcmp(main_config.output, "-") != 0) {
      out_fd = open(main_config.output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (out_fd < 0) {
        fprintf(stderr, "Cannot open output '%s': %s\n", main_config.output, strerror(errno));
        return 1;
      }
    }

    server_config cfg = {
      .port = (uint16_t)main_config.port,
      .bind_addr = main_config.addr,
      .output_fd = out_fd,
      .timeout_s = main_config.timeout
    };
    int ret = run_server(&cfg);

    if (out_fd != STDOUT_FILENO) close(out_fd);

    // Clean up output file on failure
    if (ret != 0 && main_config.output && strcmp(main_config.output, "-") != 0) unlink(main_config.output);

    return ret;
  } else { // --- CLIENT ---
    int in_fd = STDIN_FILENO;
    if (main_config.input && strcmp(main_config.input, "-") != 0) {
      in_fd = open(main_config.input, O_RDONLY);
      if (in_fd < 0) {
        fprintf(stderr, "Cannot open input '%s': %s\n", main_config.input, strerror(errno));
        return 1;
      }
    }
    
    client_config cfg = {
      .host = main_config.addr,
      .port = (uint16_t)main_config.port,
      .input_fd = in_fd,
      .timeout_s = main_config.timeout
    };
    int ret = run_client(&cfg);

    if (in_fd != STDIN_FILENO) close(in_fd);

    return ret;
  }

  return 0;
}
