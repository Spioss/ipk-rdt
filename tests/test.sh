#!/bin/bash
# test_rdt.sh
# Automated end-to-end tests for ipk-rdt
# NOTE: This test file was generated with the assistance of AI (Claude Sonnet 4.6, claude.ai).
# AI-assisted testing is permitted per project guidelines.

BINARY="./ipk-rdt"
PORT=9100
PASS=0
FAIL=0
SKIP=0
TIMEOUT=5

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m'

pass() { echo -e "  ${GREEN}[OK]${NC} $1"; PASS=$((PASS+1)); }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; FAIL=$((FAIL+1)); }
skip() { echo -e "  ${YELLOW}[SKIP]${NC} $1"; SKIP=$((SKIP+1)); }

cleanup() {
    rm -f /tmp/rdt_input.bin /tmp/rdt_received.bin /tmp/rdt_input2.bin
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    PORT=$((PORT+1))
}

start_server() {
    $BINARY -s -p $PORT -o /tmp/rdt_received.bin -w $TIMEOUT &
    SERVER_PID=$!
    sleep 0.2
}

start_server_stdout() {
    $BINARY -s -p $PORT -w $TIMEOUT > /tmp/rdt_received.bin &
    SERVER_PID=$!
    sleep 0.2
}

require_tc() {
    if ! command -v tc &>/dev/null; then
        skip "$1 (tc not available)"
        cleanup
        return 1
    fi
    if ! sudo tc qdisc show dev lo &>/dev/null; then
        skip "$1 (no sudo for tc)"
        cleanup
        return 1
    fi
    return 0
}

netem_reset() {
    sudo tc qdisc del dev lo root 2>/dev/null || true
}

# ── Test 1: Hello world ───────────────────────────────────────────────────────
echo ""
echo "Test 1: Small string transfer"
start_server
echo "hello world" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "hello world" ]; then pass "content matches"
else fail "content mismatch: got '$RESULT'"; fi
cleanup

# ── Test 2: Empty input ───────────────────────────────────────────────────────
echo ""
echo "Test 2: Empty input"
start_server
printf "" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
SIZE=$(wc -c < /tmp/rdt_received.bin 2>/dev/null)
if [ "$SIZE" = "0" ]; then pass "0 bytes received correctly"
else fail "expected 0 bytes, got $SIZE"; fi
cleanup

# ── Test 3: Single byte ───────────────────────────────────────────────────────
echo ""
echo "Test 3: Single byte transfer"
start_server
printf 'X' | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "X" ]; then pass "single byte matches"
else fail "single byte mismatch"; fi
cleanup

# ── Test 4: Exactly MAX_PAYLOAD bytes (1178) ──────────────────────────────────
echo ""
echo "Test 4: Exactly 1178 bytes (one full segment)"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1178 count=1 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "md5sum matches"
else fail "md5sum mismatch"; fi
cleanup

# ── Test 5: 1179 bytes (just over one segment) ───────────────────────────────
echo ""
echo "Test 5: 1179 bytes (just over one segment boundary)"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1179 count=1 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "md5sum matches"
else fail "md5sum mismatch"; fi
cleanup

# ── Test 6: 1KB binary ───────────────────────────────────────────────────────
echo ""
echo "Test 6: 1KB binary file"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=1 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "md5sum matches"
else fail "md5sum mismatch"; fi
cleanup

# ── Test 7: 32KB (exactly one full window) ────────────────────────────────────
echo ""
echo "Test 7: 32KB (exactly one full window = 32 segments)"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=$((32*1178)) count=1 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "md5sum matches"
else fail "md5sum mismatch"; fi
cleanup

# ── Test 8: 1MB binary ───────────────────────────────────────────────────────
echo ""
echo "Test 8: 1MB binary file"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=1024 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "md5sum matches"
else fail "md5sum mismatch"; fi
cleanup

# ── Test 9: 10MB binary ──────────────────────────────────────────────────────
echo ""
echo "Test 9: 10MB binary file"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=10240 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "md5sum matches"
else fail "md5sum mismatch"; fi
cleanup

# ── Test 10: Text file with newlines ─────────────────────────────────────────
echo ""
echo "Test 10: Text file with newlines"
printf "line1\nline2\nline3\n" > /tmp/rdt_input.bin
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
EXPECTED=$(printf "line1\nline2\nline3")
if [ "$RESULT" = "$EXPECTED" ]; then pass "text with newlines matches"
else fail "text mismatch"; fi
cleanup

# ── Test 11: Binary file with null bytes ─────────────────────────────────────
echo ""
echo "Test 11: Binary file with null bytes"
printf '\x00\x01\x02\xFF\xFE\xFD' > /tmp/rdt_input.bin
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "binary with null bytes matches"
else fail "binary mismatch"; fi
cleanup

# ── Test 12: IPv4 explicit ────────────────────────────────────────────────────
echo ""
echo "Test 12: IPv4 explicit"
start_server
echo "ipv4 test" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "ipv4 test" ]; then pass "IPv4 transfer ok"
else fail "IPv4 transfer failed"; fi
cleanup

# ── Test 13: IPv6 ─────────────────────────────────────────────────────────────
echo ""
echo "Test 13: IPv6 transfer"
$BINARY -s -p $PORT -o /tmp/rdt_received.bin -w $TIMEOUT &
SERVER_PID=$!
sleep 0.2
echo "ipv6 test" | $BINARY -c -a ::1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "ipv6 test" ]; then pass "IPv6 transfer ok"
else fail "IPv6 transfer failed: '$RESULT'"; fi
cleanup

# ── Test 14: localhost hostname ───────────────────────────────────────────────
echo ""
echo "Test 14: Hostname resolution (localhost)"
start_server
echo "hostname test" | $BINARY -c -a localhost -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "hostname test" ]; then pass "hostname resolution ok"
else fail "hostname resolution failed"; fi
cleanup

# ── Test 15: File to stdout ───────────────────────────────────────────────────
echo ""
echo "Test 15: File to stdout"
echo "stdout test" > /tmp/rdt_input.bin
start_server_stdout
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "stdout test" ]; then pass "stdout output ok"
else fail "stdout output failed: '$RESULT'"; fi
cleanup

# ── Test 16: stdin to file ────────────────────────────────────────────────────
echo ""
echo "Test 16: stdin to file"
start_server
echo "stdin test" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "stdin test" ]; then pass "stdin to file ok"
else fail "stdin to file failed"; fi
cleanup

# ── Test 17: stdin to stdout ──────────────────────────────────────────────────
echo ""
echo "Test 17: stdin to stdout"
start_server_stdout
echo "stdin stdout test" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "stdin stdout test" ]; then pass "stdin to stdout ok"
else fail "stdin to stdout failed"; fi
cleanup

# ── Test 18: Client timeout (no server) ──────────────────────────────────────
echo ""
echo "Test 18: Client timeout when no server"
echo "test" | $BINARY -c -a 127.0.0.1 -p $PORT -w 1
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then pass "client exited non-zero ($EXIT_CODE)"
else fail "client should have failed but returned 0"; fi
cleanup

# ── Test 19: Server timeout (no client) ──────────────────────────────────────
echo ""
echo "Test 19: Server timeout when no client"
$BINARY -s -p $PORT -o /tmp/rdt_received.bin -w 1
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then pass "server exited non-zero ($EXIT_CODE)"
else fail "server should have failed but returned 0"; fi
cleanup

# ── Test 20: -w flag actually affects timeout ─────────────────────────────────
echo ""
echo "Test 20: -w flag affects timeout duration"
START=$(date +%s%3N)
$BINARY -s -p $PORT -o /tmp/rdt_received.bin -w 2
END=$(date +%s%3N)
ELAPSED=$(( END - START ))
if [ $ELAPSED -ge 1800 ] && [ $ELAPSED -le 4000 ]; then
    pass "-w 2 caused ~2s timeout (${ELAPSED}ms)"
else
    fail "-w 2 timeout was ${ELAPSED}ms (expected ~2000ms)"
fi
cleanup

# ── Test 21: Server bind on specific address ──────────────────────────────────
echo ""
echo "Test 21: Server bind on specific address (127.0.0.1)"
$BINARY -s -p $PORT -a 127.0.0.1 -o /tmp/rdt_received.bin -w $TIMEOUT &
SERVER_PID=$!
sleep 0.2
echo "bind test" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "bind test" ]; then pass "bind to specific address ok"
else fail "bind to specific address failed"; fi
cleanup

# ── Test 22: Client exit code 0 on success ───────────────────────────────────
echo ""
echo "Test 22: Client exit code 0 on success"
start_server
echo "exit code test" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
EXIT_CODE=$?
wait $SERVER_PID
if [ $EXIT_CODE -eq 0 ]; then pass "client exit code 0"
else fail "client exit code was $EXIT_CODE"; fi
cleanup

# ── Test 23: Server exit code 0 on success ───────────────────────────────────
echo ""
echo "Test 23: Server exit code 0 on success"
start_server
echo "server exit test" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ]; then pass "server exit code 0"
else fail "server exit code was $EXIT_CODE"; fi
cleanup

# ── Test 24: Invalid port ─────────────────────────────────────────────────────
echo ""
echo "Test 24: Invalid port (0)"
$BINARY -s -p 0 -w 1 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then pass "invalid port rejected"
else fail "invalid port should fail"; fi
cleanup

# ── Test 25: Missing mandatory args ──────────────────────────────────────────
echo ""
echo "Test 25: Missing -p PORT"
$BINARY -s -w 1 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then pass "missing -p rejected"
else fail "missing -p should fail"; fi
cleanup

# ── Test 26: -s and -c together ──────────────────────────────────────────────
echo ""
echo "Test 26: -s and -c together"
$BINARY -s -c -p $PORT 2>/dev/null
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then pass "-s and -c together rejected"
else fail "-s and -c together should fail"; fi
cleanup

# ── Test 27: 10% packet loss ─────────────────────────────────────────────────
echo ""
echo "Test 27: 10% packet loss (tc netem)"
require_tc "10% packet loss" || { PORT=$((PORT+1)); true; } && {
    netem_reset
    sudo tc qdisc add dev lo root netem loss 10% 2>/dev/null
    dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=512 2>/dev/null
    start_server
    $BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
    wait $SERVER_PID
    netem_reset
    IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
    OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
    if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "transfer correct despite 10% loss"
    else fail "data corrupted under packet loss"; fi
    cleanup
}

# ── Test 28: Packet reordering ───────────────────────────────────────────────
echo ""
echo "Test 28: Packet reordering (tc netem)"
require_tc "packet reordering" || { PORT=$((PORT+1)); true; } && {
    netem_reset
    sudo tc qdisc add dev lo root netem delay 10ms reorder 50% 2>/dev/null
    dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=512 2>/dev/null
    start_server
    $BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
    wait $SERVER_PID
    netem_reset
    IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
    OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
    if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "transfer correct despite reordering"
    else fail "data corrupted under reordering"; fi
    cleanup
}

# ── Test 29: Packet duplication ──────────────────────────────────────────────
echo ""
echo "Test 29: Packet duplication (tc netem)"
require_tc "packet duplication" || { PORT=$((PORT+1)); true; } && {
    netem_reset
    sudo tc qdisc add dev lo root netem duplicate 20% 2>/dev/null
    dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=512 2>/dev/null
    start_server
    $BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
    wait $SERVER_PID
    netem_reset
    IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
    OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
    if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "transfer correct despite 20% duplication"
    else fail "data corrupted under duplication"; fi
    cleanup
}

# ── Test 30: Combined loss + reorder + delay ──────────────────────────────────
echo ""
echo "Test 30: Combined: 5% loss + reorder + 20ms delay (tc netem)"
require_tc "combined impairment" || { PORT=$((PORT+1)); true; } && {
    netem_reset
    sudo tc qdisc add dev lo root netem loss 5% delay 20ms reorder 30% 2>/dev/null
    dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=256 2>/dev/null
    start_server
    $BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w 10
    wait $SERVER_PID
    netem_reset
    IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
    OUT_MD5=$(md5sum /tmp/rdt_received.bin 2>/dev/null | cut -d' ' -f1)
    if [ "$IN_MD5" = "$OUT_MD5" ]; then pass "transfer correct under combined impairment"
    else fail "data corrupted under combined impairment"; fi
    cleanup
}

# ── Results ───────────────────────────────────────────────────────────────────
echo ""
echo "=== Results: ${PASS} passed, ${FAIL} failed, ${SKIP} skipped ==="
[ $FAIL -eq 0 ] && exit 0 || exit 1