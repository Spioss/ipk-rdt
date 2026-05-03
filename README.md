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