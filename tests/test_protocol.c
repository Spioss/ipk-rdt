/*
 * test_protocol.c
 * Automated tests for protocol.c (encode/decode/crc32).
 * NOTE: This test file was generated with the assistance of AI (Claude by Anthropic).
 * AI-assisted testing is permitted per project guidelines.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/protocol.h"

int passed = 0;
int failed = 0;

#define TEST(name, cond) do { \
    if (cond) { printf("  [OK] %s\n", name); passed++; } \
    else       { printf("  [FAIL] %s\n", name); failed++; } \
} while(0)

/* --- Test 1: SYN packet (no payload) --- */
void test_syn_packet(void) {
    printf("\nTest 1: SYN packet (no payload)\n");

    pkt_t orig = {0};
    orig.hdr.magic    = MAGIC;
    orig.hdr.type     = PKT_SYN;
    orig.hdr.conn_id  = 0xDEADBEEF;
    orig.hdr.seq      = 0;
    orig.hdr.ack      = 0;
    orig.hdr.data_len = 0;

    uint8_t buf[MAX_PDU];
    int len = pkt_encode(&orig, buf);
    TEST("encode returns HDR_SIZE", len == HDR_SIZE);

    pkt_t decoded = {0};
    bool ok = pkt_decode(buf, &decoded, len);
    TEST("decode successful",  ok == true);
    TEST("magic matches",      decoded.hdr.magic   == MAGIC);
    TEST("type matches",       decoded.hdr.type    == PKT_SYN);
    TEST("conn_id matches",    decoded.hdr.conn_id == 0xDEADBEEF);
    TEST("seq matches",        decoded.hdr.seq     == 0);
    TEST("data_len = 0",       decoded.hdr.data_len == 0);
}

/* --- Test 2: DATA packet (with payload) --- */
void test_data_packet(void) {
    printf("\nTest 2: DATA packet (with payload)\n");

    pkt_t orig = {0};
    orig.hdr.magic    = MAGIC;
    orig.hdr.type     = PKT_DATA;
    orig.hdr.conn_id  = 0x12345678;
    orig.hdr.seq      = 1178;
    orig.hdr.ack      = 0;
    orig.hdr.data_len = 5;
    memcpy(orig.data, "hello", 5);

    uint8_t buf[MAX_PDU];
    int len = pkt_encode(&orig, buf);
    TEST("encode returns HDR_SIZE + 5", len == HDR_SIZE + 5);

    pkt_t decoded = {0};
    bool ok = pkt_decode(buf, &decoded, len);
    TEST("decode successful",   ok == true);
    TEST("type = DATA",         decoded.hdr.type     == PKT_DATA);
    TEST("seq = 1178",          decoded.hdr.seq      == 1178);
    TEST("data_len = 5",        decoded.hdr.data_len == 5);
    TEST("payload matches",     memcmp(decoded.data, "hello", 5) == 0);
}

/* --- Test 3: Corrupted packet (bad checksum) --- */
void test_corrupted_packet(void) {
    printf("\nTest 3: Corrupted packet (bad checksum)\n");

    pkt_t orig = {0};
    orig.hdr.magic    = MAGIC;
    orig.hdr.type     = PKT_DATA;
    orig.hdr.conn_id  = 0xABCD;
    orig.hdr.seq      = 0;
    orig.hdr.data_len = 4;
    memcpy(orig.data, "test", 4);

    uint8_t buf[MAX_PDU];
    int len = pkt_encode(&orig, buf);

    /* Flip a bit in the payload to simulate corruption */
    buf[HDR_SIZE + 1] ^= 0xFF;

    pkt_t decoded = {0};
    bool ok = pkt_decode(buf, &decoded, len);
    TEST("corrupted packet rejected", ok == false);
}

/* --- Test 4: Bad magic number --- */
void test_bad_magic(void) {
    printf("\nTest 4: Bad magic number\n");

    pkt_t orig = {0};
    orig.hdr.magic    = MAGIC;
    orig.hdr.type     = PKT_SYN;
    orig.hdr.conn_id  = 1;
    orig.hdr.data_len = 0;

    uint8_t buf[MAX_PDU];
    int len = pkt_encode(&orig, buf);

    /* Overwrite magic with wrong value */
    buf[0] = 0x00;
    buf[1] = 0x00;

    pkt_t decoded = {0};
    bool ok = pkt_decode(buf, &decoded, len);
    TEST("bad magic rejected", ok == false);
}

/* --- Test 5: Packet too short to contain header --- */
void test_too_short(void) {
    printf("\nTest 5: Packet too short\n");

    /* Magic ok but buffer is shorter than HDR_SIZE */
    uint8_t buf[10] = {0x47, 0x47, 0x01};
    pkt_t decoded = {0};
    bool ok = pkt_decode(buf, &decoded, 10);
    TEST("too short packet rejected", ok == false);
}

/* --- Test 6: Maximum payload size --- */
void test_max_payload(void) {
    printf("\nTest 6: Maximum payload (%d bytes)\n", MAX_PAYLOAD);

    pkt_t orig = {0};
    orig.hdr.magic    = MAGIC;
    orig.hdr.type     = PKT_DATA;
    orig.hdr.conn_id  = 0x99999999;
    orig.hdr.seq      = 9999;
    orig.hdr.data_len = MAX_PAYLOAD;

    /* Fill payload with a repeating pattern */
    for (int i = 0; i < MAX_PAYLOAD; i++)
        orig.data[i] = (uint8_t)(i & 0xFF);

    uint8_t buf[MAX_PDU];
    int len = pkt_encode(&orig, buf);
    TEST("encode returns MAX_PDU",  len == MAX_PDU);

    pkt_t decoded = {0};
    bool ok = pkt_decode(buf, &decoded, len);
    TEST("decode successful",       ok == true);
    TEST("data_len = MAX_PAYLOAD",  decoded.hdr.data_len == MAX_PAYLOAD);
    TEST("payload matches",         memcmp(decoded.data, orig.data, MAX_PAYLOAD) == 0);
}

int main(void) {
    printf("=== Protocol tests ===\n");
    test_syn_packet();
    test_data_packet();
    test_corrupted_packet();
    test_bad_magic();
    test_too_short();
    test_max_payload();
    printf("\n=== Result: %d passed, %d failed ===\n", passed, failed);
    return failed > 0 ? 1 : 0;
}