#ifndef CONFIG_H
#define CONFIG_H
#include <signal.h>

typedef struct {
  int mode; /* 1 = server, 2 = client */
  const char *addr;
  int port;
  const char *input;
  const char *output;
  int timeout;
} cfg;

volatile sig_atomic_t g_terminated = 0;


#endif /* CONFIG_H */