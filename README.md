# ipk-rdt — Reliable Data Transfer over UDP

**Author:** Lukas Mader  
**Course:** IPK 2024/2025 — Project 2  

---

## Overview

`ipk-rdt` is a command-line tool that implements reliable, ordered byte-stream transfer over UDP. It consists of a single executable that runs in either server (receiver) or client (sender) mode. The underlying transport protocol — referred to here as the **GG protocol** (magic bytes `0x4747`) — is a custom application-level reliable transport inspired by TCP, implementing a sliding window, cumulative acknowledgements, CRC32 integrity checking, and RFC 6298 adaptive retransmission timers.

---

## Build and Run

### Requirements

- GCC with C17 support (`-std=c17`)

### Build

```sh
make
```

The binary `ipk-rdt` is placed in the repository root.

### Run

**Server (receiver):**
```sh
./ipk-rdt -s -p PORT [-a ADDRESS] [-o OUTPUT] [-w TIMEOUT]
```

**Client (sender):**
```sh
./ipk-rdt -c -a HOST -p PORT [-i INPUT] [-w TIMEOUT]
```

| Option | Description |
|--------|-------------|
| `-s` | Start in server (receiver) mode |
| `-c` | Start in client (sender) mode |
| `-p PORT` | UDP port number (1–65535) |
| `-a ADDRESS/HOST` | Bind address (server) or destination host (client) |
| `-o OUTPUT` | Output file; `-` or omitted means stdout |
| `-i INPUT` | Input file; `-` or omitted means stdin |
| `-w TIMEOUT` | Progress timeout in seconds (default: 1) |
| `-h`, `--help` | Show usage and exit with code 0 |

### Examples

```sh
# File to file
./ipk-rdt -s -p 9000 -o received.bin
./ipk-rdt -c -a 127.0.0.1 -p 9000 -i input.bin

# stdin to stdout
./ipk-rdt -s -p 9000
printf 'IPK\n' | ./ipk-rdt -c -a 127.0.0.1 -p 9000

# IPv6
./ipk-rdt -s -p 9000 -o received.bin
echo "hello" | ./ipk-rdt -c -a ::1 -p 9000
```

---

## Protocol Design

### Packet Header Format

Every protocol data unit (PDU) consists of a fixed 22-byte header followed by an optional payload:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            Magic (0x4747)     |     Type      |               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               +
|                        Connection ID (32 bits)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Sequence Number (32 bits)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Acknowledgement Number (32 bits)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Data Length (16 bits) |       Checksum (32 bits)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               +---------------+
|               Checksum (cont.)                |    Padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Payload (0–1178 bytes)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Size | Description |
|-------|------|-------------|
| Magic | 2 B | Always `0x4747` ("GG"); identifies the protocol |
| Type | 1 B | Packet type (see below) |
| Connection ID | 4 B | Random 32-bit value identifying the session |
| Sequence Number | 4 B | Byte offset of the first payload byte |
| Acknowledgement | 4 B | Cumulative: acknowledges all bytes up to (not including) this offset |
| Data Length | 2 B | Payload length in bytes (0 for control packets) |
| Checksum | 4 B | CRC32 over the full PDU with checksum field zeroed |
| Padding | 1 B | Reserved, set to zero |

**Packet types:**

| Value | Name | Direction |
|-------|------|-----------|
| 1 | `SYN` | Client → Server |
| 2 | `SYNACK` | Server → Client |
| 3 | `ACK` | Both |
| 4 | `DATA` | Client → Server |
| 5 | `FIN` | Client → Server |
| 6 | `FINACK` | Server → Client |

Maximum PDU size is **1200 bytes** (header 22 B + payload up to **1178 B**).  
All multi-byte fields are encoded in **big-endian** (network byte order).

### Integrity Protection

The checksum field holds a **CRC32** computed over the entire PDU (header + payload) with the checksum field set to zero before computation. Any packet with an incorrect checksum, wrong magic, or truncated header is silently discarded.

### Connection Identification

Before sending the first SYN, the client generates a **random 32-bit connection ID** using `rand()`. Every subsequent packet for that session carries the same `conn_id`. The server records the `conn_id` from the first SYN and rejects packets with a different `conn_id`. This prevents accidental mixing of packets from different transfers and provides basic protection against stale packets from prior sessions.

Both the client's and server's initial sequence numbers are also generated randomly with `rand()`.

---

## Session Establishment and Termination

### Three-Way Handshake (SYN → SYN-ACK → ACK)

```
Client                          Server
  |                                |
  |  SYN (seq=ISN_c)               |
  |------------------------------->|
  |                                |  SS_SYN_RECEIVED
  |  SYNACK (seq=ISN_s, ack=ISN_c+1)|
  |<-------------------------------|
  |                                |
  |  ACK (ack=ISN_s+1)             |
  |------------------------------->|
  |                                |  SS_TRANSFERRING
  |   <DATA TRANSFER>              |
```

The server retransmits SYN-ACK with exponential backoff if it does not receive the final ACK within the RTO. The client retransmits SYN similarly.

### Session Teardown (FIN → FIN-ACK → ACK)

```
Client                          Server
  |                                |
  |  FIN (seq=total_bytes)         |
  |------------------------------->|
  |                                |  SS_FIN_RECEIVED
  |  FINACK (ack=total_bytes+1)    |
  |<-------------------------------|
  |                                |
  |  ACK                           |
  |------------------------------->|
  |                                |  SS_DONE → exit 0
  CS_DONE → exit 0
```

The client waits 50 ms after sending the final ACK to allow it to be received before closing. The server retransmits FIN-ACK with exponential backoff until the final ACK arrives or the timeout expires; if it times out while waiting for the final ACK it still exits with code 0 (transfer is considered complete).

---

## Sequencing and Acknowledgement

**Byte-stream sequencing** is used: `seq` in a DATA packet is the byte offset of the first byte in the payload, identical to TCP. For example, if the client sends 1178 bytes starting at offset 0, the next packet's `seq` is 1178.

**Cumulative acknowledgements**: the `ack` field in an ACK packet means "I have received all bytes up to (not including) this offset." The server sends a cumulative ACK after every DATA packet it processes.

The client maintains a **sliding send window** of `WINDOW_SIZE = 32` slots. Each slot holds one segment, its sequence number, length, send timestamp, current RTO, and retransmit count. The window allows up to `32 × 1178 = 37 696` bytes of unacknowledged data in flight simultaneously.

---

## Retransmission Strategy and Timeout Handling

### Adaptive RTO — RFC 6298 (Jacobson Algorithm)

The client tracks RTT per connection and computes RTO adaptively:

```
First sample:
  SRTT   = RTT_sample
  RTTVAR = RTT_sample / 2

Subsequent samples:
  RTTVAR = (1 - 0.25) * RTTVAR + 0.25 * |SRTT - RTT_sample|
  SRTT   = (1 - 0.125) * SRTT  + 0.125 * RTT_sample
  RTO    = SRTT + 4 * RTTVAR
```

RTO is clamped to the range **[50 ms, 10 000 ms]**; the initial RTO before the first sample is **200 ms**.

**Karn's algorithm** is applied: RTT samples are taken only from segments that were not retransmitted, to avoid ambiguity.

### Retransmission Behaviour

- Each send-window slot has its own per-segment RTO timer.
- If a segment's timer expires, it is retransmitted and its RTO is doubled (exponential backoff).
- The global **progress timeout** (`-w TIMEOUT` seconds without any new ACK) terminates the transfer with a non-zero exit code.
- SYN and SYN-ACK retransmissions use the same initial RTO and the same exponential backoff.
- The event loop uses `select(2)` with a dynamically computed timeout equal to the nearest pending retransmit deadline (or 10 ms for the client, 50 ms for the server as a floor), avoiding busy-waiting.

---

## Duplicate and Out-of-Order Packet Handling

The server maintains a **receive buffer** of `WINDOW_SIZE = 32` slots indexed by `(seq / MAX_PAYLOAD) % WINDOW_SIZE`. Out-of-order segments are stored in the buffer without being written to output. When a contiguous run of segments starting at `rcv_expected` becomes available, they are written in order and `rcv_expected` is advanced.

Duplicate packets (seq < `rcv_expected`) are silently dropped, but an ACK is sent back so the client can advance its window. Packets outside the receive window (`seq >= rcv_expected + WINDOW_SIZE * MAX_PAYLOAD`) are also discarded.

The client deduplicates ACKs by ignoring any ACK with `ack <= send_base`.

---

## Segment Size and Window Behaviour

| Parameter | Value |
|-----------|-------|
| Max PDU | 1200 bytes |
| Header size | 22 bytes |
| Max payload | 1178 bytes |
| Window size | 32 segments |
| Max in-flight bytes | 37 696 bytes |

The 1200-byte PDU limit reduces IP fragmentation risk on both IPv4 and IPv6 paths. The 32-segment window allows efficient pipelining without overloading the receiver.

---

## Testing

### Running Tests

```sh
# End-to-end integration tests (requires ./ipk-rdt binary)
make test

# Protocol unit tests (encode/decode/checksum)
make test-protocol
```

### Test Environment

- OS: Linux (Arch Linux, kernel 6.x)
- Architecture: x86\_64
- Loopback interface (127.0.0.1 / ::1) used for all local tests
- `tc netem` used for network impairment tests (requires `sudo`)

### Test Coverage

#### Unit Tests (`tests/test_protocol.c`)

Tests exercise `pkt_encode` / `pkt_decode` / `crc32` directly:

| # | What | Why | Expected |
|---|------|-----|----------|
| 1 | SYN packet (no payload) | Verify control-packet round-trip | Decoded fields match original |
| 2 | DATA packet with 5-byte payload | Verify payload encoding | Payload byte-for-byte identical |
| 3 | Corrupted payload (bit flip) | Detect integrity failure | `pkt_decode` returns false |
| 4 | Wrong magic number | Reject unrelated datagrams | `pkt_decode` returns false |
| 5 | Buffer shorter than header | Guard against truncated packets | `pkt_decode` returns false |
| 6 | Max payload (1178 bytes) | Boundary check | Full round-trip succeeds |

#### Integration Tests (`tests/test.sh`)

| # | What | Why | Condition |
|---|------|-----|-----------|
| 1 | Small string ("hello world") | Basic correctness | Content matches exactly |
| 2 | Empty input | Edge case — zero-byte transfer | 0 bytes received |
| 3 | 1 KB random binary | Binary data correctness | MD5 matches |
| 4 | 1 MB random binary | Larger transfer, multi-segment window | MD5 matches |
| 5 | 10 MB random binary | Stream throughput | MD5 matches |
| 6 | IPv6 (::1) | IPv6 code path | Content matches |
| 7 | File to stdout | stdout output mode | Content matches |
| 8 | Client timeout (no server) | Timeout termination | Non-zero exit code |
| 9 | Server timeout (no client) | Timeout termination | Non-zero exit code |
| 10 | 10% packet loss (`tc netem`) | Retransmission correctness | MD5 matches |
| 11 | 50% reorder + 10 ms delay (`tc netem`) | Out-of-order handling | MD5 matches |

Tests 10 and 11 require `tc` and `sudo`; they are skipped automatically when unavailable.

### Sample Output

```
Test 1: Small string transfer
  [OK] content matches

Test 2: Empty input
  [OK] empty file received correctly

Test 3: 1KB binary file
  [OK] md5sum matches

Test 4: 1MB binary file
  [OK] md5sum matches

Test 5: 10MB binary file
  [OK] md5sum matches

Test 6: IPv6 transfer
  [OK] IPv6 transfer ok

Test 7: File to stdout
  [OK] stdout output ok

Test 8: Client timeout when no server
  [OK] client exited with non-zero code (1)

Test 9: Server timeout when no client
  [OK] server exited with non-zero code (1)

=== Results: 9 passed, 0 failed ===
```

---

## State Machines

### Server State Machine

```
SS_LISTENING
    │  (SYN received)
    ▼
SS_SYN_RECEIVED   ←── retransmit SYN-ACK on timeout
    │  (ACK received)
    ▼
SS_TRANSFERRING   ←── receive DATA, buffer, write, send ACK
    │  (FIN received, all data delivered)
    ▼
SS_FIN_RECEIVED   ←── retransmit FIN-ACK on timeout
    │  (final ACK received, or timeout)
    ▼
SS_DONE  →  exit 0
```

### Client State Machine

```
CS_CONNECTING   ←── retransmit SYN on timeout
    │  (SYN-ACK received)
    ▼
CS_TRANSFERRING ←── fill window, retransmit expired slots
    │  (EOF + all ACKed)
    ▼
CS_FIN_WAIT     ←── retransmit FIN on timeout
    │  (FIN-ACK received)
    ▼
CS_DONE  →  exit 0
```

---

## Known Limitations

- **Single transfer per process**: the server handles exactly one session and terminates, as required by the specification.
- **No parallel clients**: only one client can connect per server run.
- **No congestion control**: the window is fixed at 32 segments. Under severe congestion this may lead to suboptimal performance, though retransmission with exponential backoff prevents indefinite flooding.
- **No transfer resume**: if the process is killed mid-transfer, the partial output file is deleted on the server side.
- **Sequence number wrap-around**: the 32-bit sequence number space can theoretically wrap for transfers larger than ~4 GB. This case is not handled.

---

## References

1. POSTEL, J. *RFC 768: User Datagram Protocol*. Internet Engineering Task Force, 1980.  
   Online: https://datatracker.ietf.org/doc/html/rfc768

2. POSTEL, J. *RFC 793: Transmission Control Protocol*. Internet Engineering Task Force, 1981.  
   Online: https://datatracker.ietf.org/doc/html/rfc793

3. ALLMAN, M., PAXSON, V., BLANTON, E. *RFC 6298: Computing TCP's Retransmission Timer*. Internet Engineering Task Force, 2011.  
   Online: https://datatracker.ietf.org/doc/html/rfc6298

4. KUROSE, J. F., ROSS, K. W. *Computer Networking: A Top-Down Approach*. 8th ed. Pearson, 2021.

5. Linux manual page: `tc-netem(8)`. Online: https://man7.org/linux/man-pages/man8/tc-netem.8.html
