#ifndef NET_H
#define NET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "protocol.h"

/* Generic address container (works for both IPv4 and IPv6) */
typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} addr;

// Create a bound UDP server socket. If bind_addr is NULL, binds to all interfaces.
// Returns fd on success, -1 on error. 
int create_server(const char *bind_addr, uint16_t port);

// Resolve host:port and create connected UDP client socket.
// Fills out *srv_addr with the server address.
// Returns fd on success, -1 on error. */
int create_client(const char *host, uint16_t port, addr *srv_addr);

// end encoded pkt to addr. Returns bytes sent or -1.
int send_pkt(int fd, const pkt *pkt, const addr *dst);

// Receive a packet. Fills pkt and src. Returns true if packet valid. 
bool recieve_pkt(int fd, pkt *pkt, addr *src);

// Send a control packet (no payload)
void send_controlPkt(int fd, uint8_t type, uint32_t conn_id, uint32_t seq, uint32_t ack, const addr *dst);

// Clamp select timeout to nearest retransmit deadline 
void clamp_tv(struct timeval *tv, long rto, const struct timespec *ts);

// Compare two addr_t for equality (address and port).
bool addr_equal(const addr *a, const addr *b);

// Return milliseconds elapsed since ts (using CLOCK_MONOTONIC).
long elapsed_ms(const struct timespec *ts);

// Get current CLOCK_MONOTONIC time.
void now_ts(struct timespec *ts);

// ms_diff: compute a - b in milliseconds
long ts_diff_ms(const struct timespec *a, const struct timespec *b);

#endif /* NET_H */
