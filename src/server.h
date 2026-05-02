#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

typedef struct {
    const char *bind_addr;
    uint16_t port;
    int output_fd; // 1 - stdout | 3 - files succs open
    int timeout_s;
} server_config;

int run_server(const server_config *cfg);

#endif