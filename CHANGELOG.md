# Changelog

## [1.0.0] — 2025-05-03

### Implemented

- Client and server mode in a single executable (`ipk-rdt`)
- Custom reliable transport protocol over UDP (GG protocol, magic `0x4747`)
- Three-way handshake (SYN → SYNACK → ACK)
- Graceful session teardown (FIN → FINACK → ACK)
- Sliding window with 32 slots and pipelined transmission
- Cumulative acknowledgements
- CRC32 integrity protection over full PDU
- Out-of-order packet buffering and in-order delivery
- Duplicate packet detection and handling
- Retransmission with exponential backoff
- Adaptive RTO using Jacobson/Karels algorithm (RFC 6298)
- Karn's algorithm for RTT measurement
- IPv4 and IPv6 support (dual-stack)
- Input from file or stdin
- Output to file or stdout
- Configurable progress timeout (`-w`)
- Signal handling (SIGINT, SIGTERM) — clean termination
- Temporary file cleanup on failure

### Known Limitations

- The server handles exactly one transfer per process run
- No parallel client connections
- No congestion control beyond exponential backoff
- No transfer resume after process crash
- Sequence number wrap-around not handled (transfers > ~4 GB)
