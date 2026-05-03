#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include "netUtils.h"
#include "protocol.h"

typedef struct {
    const char      *host;
    uint16_t        port;
    int             input_fd; // 1 stdin | 3 file succs open
    int             timeout_s;
} client_config;


// Client states
typedef enum {
    CS_CONNECTING, // waiting SYN-ACK
    CS_TRANSFERRING, // waiting ACK
    CS_FIN_WAIT, // waiting FIN-ACK
    CS_DONE
} client_state;

// one slot in the send window 
typedef struct {
    uint8_t         data[MAX_PAYLOAD];
    uint32_t        seq;
    uint16_t        len;
    struct timespec sent_at;
    long            rto_ms;
    int             retransmits;
    bool            in_use;
} send_slot;

// RTT estimator (RFC 6298)
typedef struct {
    double srtt;
    double rttvar;
    long   rto_ms;
    bool   seeded;
} rtt;

// client context
typedef struct {
    client_state    state; // current state 
    uint32_t        conn_id; // id of the connection
    uint32_t        client_initial; // client initial sequence number
    uint32_t        server_initial; // server initial sequence number
    addr            srv_addr; // server addresss

    send_slot       window[WINDOW_SIZE];
    uint32_t        send_base;   /* oldest un-acked byte */
    uint32_t        send_next;   /* next byte to send */
    uint32_t        total_read;  /* total bytes read from input */
    bool            eof;         /* no more input data */

    rtt           rtt; 

    struct timespec syn_sent_ts;
    long            syn_rto;

    struct timespec progress_ts;

    int             input_fd;
    int             timeout_s;
} client_ctx;

int run_client(const client_config *cfg);

#endif