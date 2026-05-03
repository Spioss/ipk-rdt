#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include "netUtils.h"
#include "protocol.h"

typedef struct {
    const char *bind_addr;
    uint16_t port;
    int output_fd; // 1 - stdout | 3 - files succs open
    int timeout_s;
} server_config;

// Receive-window slot
typedef struct {
    uint8_t data[MAX_PAYLOAD];
    uint16_t len;
    bool occupied;
} rcv_slot;

// STATE MACHINE FOR SERVER
typedef enum {
    SS_LISTENING,       // waitin for first SYN
    SS_SYN_RECEIVED, // got SYN, send SYN-ACK, wait for ACK
    SS_TRANSFERRING, // data transfer
    SS_FIN_RECEIVED, // got FIN, send FIN-ACK
    SS_DONE          // done
} server_state;


// All server runtime state 
typedef struct {
    server_state state; // currenbt state 
    uint32_t conn_id; // id of the connection
    uint32_t client_initial; // client initial sequence number
    uint32_t server_initial; // server initial sequence number
    addr client_addr; // client addresss
 
    rcv_slot rcv_buf[WINDOW_SIZE]; // recieve window for out of order data pkts
    uint32_t rcv_expected; // next expected byte offset

    bool synack_pending; // retransmition of synack 
    long synack_rto; //max
    struct timespec synack_ts;

    bool finack_sent; // retransmition of finack
    long finack_rto; //max
    struct timespec finack_ts;
    uint32_t fin_seq;

    struct timespec progress_ts; 

    int output_fd;
    int timeout_s; // value from -w
} server_ctx;

int run_server(const server_config *cfg);

#endif