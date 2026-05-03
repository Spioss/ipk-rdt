#include <stdint.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "protocol.h"

// CRC32 algorithm
// Computes a CRC32 checksum for packet integrity verification.
// Uses the standard reversed CRC-32 polynomial 0xEDB88320.

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init(void){
  for(int i = 0; i < 256; i++){
    uint32_t crc = (uint32_t)i;
    for(int j = 0; j < 8; j++){
      if(crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320u; // shift and reversed polynomial
      else
        crc = crc >> 1; // shift 
    }
    crc32_table[i] = crc;
  }
  crc32_table_ready = true;
}

uint32_t crc32(const uint8_t *data, int len){
  if (!crc32_table_ready) crc32_init();

  uint32_t crc = 0xFFFFFFFFu;
  for(int i = 0; i < len; i++){
    // XOR the LSB byte with data byte
    uint8_t idx = (uint8_t)(crc ^ data[i]);
    crc = crc32_table[idx] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu; // inversion  - standard CRC32
}


//helpers functions for writing big-endian values to a buffer
//writes to buffer
static void write_u16(uint8_t *buf, uint16_t value){
  buf[0] = (value >> 8) & 0xFF;
  buf[1] = (value) & 0xFF;
}
static void write_u32(uint8_t *buf, uint32_t value){
  buf[0] = (value >> 24) & 0xFF;
  buf[1] = (value >> 16) & 0xFF;
  buf[2] = (value >> 8) & 0xFF;
  buf[3] = (value) & 0xFF;
}

//reads from buffer
static uint16_t read_u16(const uint8_t *buf){
  return ((uint16_t)buf[0] << 8) | buf[1];
}
static uint32_t read_u32(const  uint8_t *buf){
  return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] <<  8) | (uint32_t)buf[3];
}

// pkt_encode
// takes a `pkt_t` (C structure) and writes it to a byte array in big-endian format
// Big-endian = the most significant byte comes first 
bool pkt_decode(const uint8_t *buf, pkt_t *pkt, int len){
  if(len < HDR_SIZE) return false; // not even complete header

  uint16_t magic_number = read_u16(buf + 0);
  if (magic_number != MAGIC) return false;

  uint16_t data_len = read_u16(buf + 15);
  if(data_len > MAX_PAYLOAD) return false;
  if(len < HDR_SIZE + data_len) return false; 

  // verify checksum
  uint32_t recieved_checksum = read_u32(buf + 17);
  uint8_t tmp[MAX_PDU];
  int total = HDR_SIZE + data_len;
  memcpy(tmp, buf, total);
  write_u32(tmp + 17, 0);
  uint32_t calculate_checksum = crc32(tmp, total);
  if(calculate_checksum != recieved_checksum) return false;

  // decode into struct
  pkt->hdr.magic = magic_number;
  pkt->hdr.type = buf[2];
  pkt->hdr.conn_id = read_u32(buf + 3);
  pkt->hdr.seq = read_u32(buf + 7);
  pkt->hdr.ack = read_u32(buf + 11);
  pkt->hdr.data_len = data_len;
  pkt->hdr.checksum = recieved_checksum;
  pkt->hdr.padding = buf[21];

  if(data_len > 0) memcpy(pkt->data, buf + HDR_SIZE, data_len); // payload copy

  return true;
}

int pkt_encode(const pkt_t *pkt, uint8_t *buf){
  int total = HDR_SIZE + pkt->hdr.data_len;

  write_u16(buf + 0, pkt->hdr.magic);
  buf[2] = pkt->hdr.type;
  write_u32(buf + 3, pkt->hdr.conn_id);
  write_u32(buf + 7, pkt->hdr.seq);
  write_u32(buf + 11, pkt->hdr.ack);
  write_u16(buf + 15, pkt->hdr.data_len);
  write_u32(buf + 17, 0); // placeholder
  buf[21] = pkt->hdr.padding;

  if (pkt->hdr.data_len > 0) memcpy(buf + HDR_SIZE, pkt->data, pkt->hdr.data_len);

  uint32_t checksum = crc32(buf, total);
  write_u32(buf + 17, checksum);

  return total;
}