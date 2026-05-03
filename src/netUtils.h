#ifndef NET_H
#define NET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "protocol.h"

// address container (can contain ipv4 and ipv6)
typedef struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
} addr;

// create a bound UDP server socket, if bind_addr is NULL, binds to all interfaces.
// returns fd on success, -1 on error. 
int create_server(const char *bind_addr, uint16_t port);

// resolve host:port and create connected UDP client socket.
// fills out *srv_addr with the server address.
// returns fd on success, -1 on error. */
int create_client(const char *host, uint16_t port, addr *srv_addr);

// end encoded pkt to addr, returns bytes sent or -1.
int send_pkt(int fd, const pkt *pkt, const addr *dst);

// receive a packet. fills pkt and src. returns true if packet valid. 
bool recieve_pkt(int fd, pkt *pkt, addr *src);

// send a control packet (no payload)
void send_controlPkt(int fd, uint8_t type, uint32_t conn_id, uint32_t seq, uint32_t ack, const addr *dst);

// clamp select timeout to nearest retransmit deadline 
void clamp_tv(struct timeval *tv, long rto, const struct timespec *ts);

// compare two addr_t for equality (address and port).
bool addr_equal(const addr *a, const addr *b);

// return milliseconds elapsed since ts (using CLOCK_MONOTONIC).
long elapsed_ms(const struct timespec *ts);

// get current CLOCK_MONOTONIC time.
void now_ts(struct timespec *ts);

// ms_diff: compute a - b in milliseconds
long ts_diff_ms(const struct timespec *a, const struct timespec *b);

#endif /* NET_H */
