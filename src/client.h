#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>

typedef struct {
    const char *host;
    uint16_t port;
    int input_fd; // 1 stdin | 3 file succs open
    int timeout_s;
} client_config;

int run_client(const client_config *cfg);

#endif