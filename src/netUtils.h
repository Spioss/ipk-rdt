#ifndef NET_H
#define NET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "protocol.h"

/* Generic address container (works for both IPv4 and IPv6) */
typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} addr_t;

// Create a bound UDP server socket. If bind_addr is NULL, binds to all interfaces.
// Returns fd on success, -1 on error. 
int create_server(const char *bind_addr, uint16_t port);

// Resolve host:port and create connected UDP client socket.
// Fills out *srv_addr with the server address.
// Returns fd on success, -1 on error. */
int create_client(const char *host, uint16_t port, addr_t *srv_addr);

// end encoded pkt to addr. Returns bytes sent or -1.
int send(int fd, const pkt_t *pkt, const addr_t *dst);

// Receive a packet. Fills pkt and src. Returns true if packet valid. 
bool recieve(int fd, pkt_t *pkt, addr_t *src);

// Compare two addr_t for equality (address and port).
bool addr_equal(const addr_t *a, const addr_t *b);

// Return milliseconds elapsed since ts (using CLOCK_MONOTONIC).
long elapsed_ms(const struct timespec *ts);

// Get current CLOCK_MONOTONIC time.
void now_ts(struct timespec *ts);

// ms_diff: compute a - b in milliseconds
long ts_diff_ms(const struct timespec *a, const struct timespec *b);

#endif /* NET_H */
