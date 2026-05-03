# ipk-rdt

## server things
while (!g_terminated)
  │
  ├── vypocitaj select timeout (50ms alebo kratsi ak retransmisia)
  │
  ├── select() — cakaj na paket alebo timeout
  │
  ├── ak prisel paket → process_packet()
  │     └── handle_listen / handle_syn_received /
  │         handle_transferring / handle_fin_received
  │
  ├── handle_retransmits() — posli znova ak treba
  │
  ├── check_progress_timeout() — globálny timeout
  │
  └── ak SS_DONE → exit 0

## client things
while (!g_terminated)
  │
  ├── compute_timeout() - najkratsi timeout
  │
  ├── select() - cakaj na paket alebo timeout
  │
  ├── process_packet() - SYN-ACK / ACK / FIN-ACK
  │
  ├── CS_CONNECTING  → retransmit SYN
  │
  ├── CS_TRANSFERRING → fill_window
  │                  → retransmit_window
  │                  → ak eof + all acked → posli FIN
  │
  ├── CS_FIN_WAIT   → retransmit_fin
  │
  └── CS_DONE       → return 0

  Pred ACK=3534:
window: [0][1178][2356][3534][4712]
         ^
      send_base=0

Po ACK=3534:
window: [  ][    ][    ][3534][4712]
                          ^
                       send_base=3534