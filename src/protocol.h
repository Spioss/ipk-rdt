#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// Magic bytes: "GG" good game :D
#define MAGIC           0x4747

// Packet types
#define PKT_SYN         1
#define PKT_SYNACK      2
#define PKT_ACK         3
#define PKT_DATA        4
#define PKT_FIN         5
#define PKT_FINACK      6

// Sizes
#define HDR_SIZE        22
#define MAX_PDU         1200
#define MAX_PAYLOAD     (MAX_PDU - HDR_SIZE)   /* 1178 bytes */

// Sliding window
#define WINDOW_SIZE     32

// Retransmission 
#define RTO_INITIAL_MS  200 // first timeout
#define RTO_MIN_MS      50 // min time 50ms
#define RTO_MAX_MS      10000 //max 10s
#define RTO_ALPHA       0.125 // weight of the new RTT sample (1/8)
#define RTO_BETA        0.25 // weight of the new deviation  (1/4)


typedef struct {
    uint16_t magic; // 2B - magic number
    uint8_t  type; // 1B - type of pkt
    uint32_t conn_id; // 4B id of connection (random)
    uint32_t seq; // 4B byte sequence 
    uint32_t ack; // 4B acnkowledges bytes up to this number
    uint16_t data_len; // 2B payload lenght
    uint32_t checksum; // 4b check sum
    uint8_t  padding; // padding (for 22 bytes)
} pkt_hdr; // TOTAL 22b = header size

/* A full protocol data unit with header + payload */
typedef struct {
    pkt_hdr hdr;
    uint8_t   data[MAX_PAYLOAD];
} pkt;

// encode pkt (hdr + data) into buf (wire format). buf must be at least HDR_SIZE + hdr.data_len bytes.
int  pkt_encode(const pkt *pkt, uint8_t *buf);

// decode buf (wire format) into pkt, returns true if valid (magic + checksum ok).
bool pkt_decode(const uint8_t *buf, pkt *pkt, int len);

// compute CRC32 over data of given length
uint32_t crc32(const uint8_t *data, int len);

#endif /* PROTOCOL_H */
