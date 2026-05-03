#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "netUtils.h"
#include "protocol.h"


static int create_server(const char *bind_addr, uint16_t port){
  struct addrinfo hints, *res, *rp;

  // port -> string for getaddrinfo
  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // ipv4/ipv6
  hints.ai_socktype = SOCK_DGRAM; // udp
  hints.ai_flags = AI_PASSIVE; // for server bind()

  int response = getaddrinfo(bind_addr, port_str, &hints, &res);
  if (response != 0) {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(response));
      return -1;
  }

  // Preafer Ipv6
  struct addrinfo *first_ipv6 = NULL;
  struct addrinfo *first_ipv4 = NULL;
  for(rp = res; rp != NULL; rp = rp->ai_next){
    if(rp->ai_family == AF_INET6 && first_ipv6 == NULL) first_ipv6 = rp; // IPV6
    if(rp->ai_family == AF_INET && first_ipv4 == NULL) first_ipv4 = rp; // IPV4
  }
  struct addrinfo *order[2] = { first_ipv6, first_ipv4 };

  // try to create and bind ipv6 socket then ipv4 socket
  int fd_socket = -1;
  for(int i = 0; i < 2; i++){
    rp = order[i];
    if(!rp) continue;

    // create a socket
    fd_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd_socket < 0 ) continue;

    setsockopt(fd_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)); // allows the port to be reused immediately

    // Dual-stack: also accept IPv4-mapped connections
    if (rp->ai_family == AF_INET6) setsockopt(fd_socket, IPPROTO_IPV6, IPV6_V6ONLY, &(int){0}, sizeof(int));

    if(bind(fd_socket, rp->ai_addr, rp->ai_addrlen) == 0) break; // binded socket

    close(fd_socket); // bind not done -> next socket file desc
    fd_socket = -1;
  }
  
  freeaddrinfo(res);
  if (fd_socket < 0) {
    fprintf(stderr, "Cannot bind to port %u: %s\n", port, strerror(errno));
    return -1;
  }

  return fd_socket;
}

int create_client(const char *host, uint16_t port, addr_t *srv_addr){
  struct addrinfo hints, *res, *rp;
  // port -> string for getaddrinfo
  char port_str[8];
  snprintf(port_str, sizeof(port_str), "%u", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // ipv4/ipv6
  hints.ai_socktype = SOCK_DGRAM; // udp

  int response = getaddrinfo(host, port_str, &hints, &response);
  if(response != 0){ 
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(response));
    return -1;
  }  

  int fd_socket = -1;
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    fd_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd_socket < 0) continue;

    // Save server address 
    memcpy(&srv_addr->addr, rp->ai_addr, rp->ai_addrlen);
    srv_addr->addrlen = rp->ai_addrlen;
    break;
    }

    freeaddrinfo(res);

    if (fd_socket < 0) {
      fprintf(stderr, "could not create UDP socket: %s\n", strerror(errno));
      return -1;
    }

    return fd_socket;
}

int send(int fd, const pkt_t *pkt, const addr_t *dst){

}


bool recieve(int fd, pkt_t *pkt, addr_t *src){

}



// --- TIME ---
void now_ts(struct timespec *ts){
  clock_gettime(CLOCK_MONOTONIC, ts);
}

long ts_diff_ms(const struct timespec *a, const struct timespec *b){
  long sec = a->tv_sec - b->tv_sec;
  long nsec = a->tv_nsec - b->tv_nsec;
  return sec * 1000L + nsec / 1000000L;
}

long elapsed_ms(const struct timespec *ts){
  struct timespec now;
  now_ts(&now);
  return ts_diff_ms(&now, ts);
}



