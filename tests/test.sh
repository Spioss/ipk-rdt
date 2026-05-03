#!/bin/bash
# test_rdt.sh
# Automated tests for ipk-rdt
# NOTE: This test file was generated with the assistance of AI (Claude by Anthropic).

BINARY="./ipk-rdt"
PORT=9100
PASS=0
FAIL=0
TIMEOUT=5

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

pass() { echo -e "  ${GREEN}[OK]${NC} $1"; PASS=$((PASS+1)); }
fail() { echo -e "  ${RED}[FAIL]${NC} $1"; FAIL=$((FAIL+1)); }

cleanup() {
    rm -f /tmp/rdt_input.bin /tmp/rdt_received.bin /tmp/rdt_empty.bin
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
    PORT=$((PORT+1))
}

start_server() {
    $BINARY -s -p $PORT -o /tmp/rdt_received.bin -w $TIMEOUT &
    SERVER_PID=$!
    sleep 0.2  # let server start
}

# ── Test 1: Hello world (small string) ───────────────────────────────────────
echo ""
echo "Test 1: Small string transfer"
start_server
echo "hello world" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "hello world" ]; then
    pass "content matches"
else
    fail "content mismatch: got '$RESULT'"
fi
cleanup

# ── Test 2: Empty input ───────────────────────────────────────────────────────
echo ""
echo "Test 2: Empty input"
start_server
echo -n "" | $BINARY -c -a 127.0.0.1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
SIZE=$(wc -c < /tmp/rdt_received.bin 2>/dev/null)
if [ "$SIZE" = "0" ]; then
    pass "empty file received correctly"
else
    fail "expected 0 bytes, got $SIZE"
fi
cleanup

# ── Test 3: 1KB binary file ───────────────────────────────────────────────────
echo ""
echo "Test 3: 1KB binary file"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=1 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then
    pass "md5sum matches"
else
    fail "md5sum mismatch: $IN_MD5 != $OUT_MD5"
fi
cleanup

# ── Test 4: 1MB binary file ───────────────────────────────────────────────────
echo ""
echo "Test 4: 1MB binary file"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=1024 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then
    pass "md5sum matches"
else
    fail "md5sum mismatch"
fi
cleanup

# ── Test 5: 10MB binary file ──────────────────────────────────────────────────
echo ""
echo "Test 5: 10MB binary file"
dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=10240 2>/dev/null
start_server
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
OUT_MD5=$(md5sum /tmp/rdt_received.bin | cut -d' ' -f1)
if [ "$IN_MD5" = "$OUT_MD5" ]; then
    pass "md5sum matches"
else
    fail "md5sum mismatch"
fi
cleanup

# ── Test 6: IPv6 ──────────────────────────────────────────────────────────────
echo ""
echo "Test 6: IPv6 transfer"
$BINARY -s -p $PORT -o /tmp/rdt_received.bin -w $TIMEOUT &
SERVER_PID=$!
sleep 0.2
echo "hello ipv6" | $BINARY -c -a ::1 -p $PORT -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin 2>/dev/null)
if [ "$RESULT" = "hello ipv6" ]; then
    pass "IPv6 transfer ok"
else
    fail "IPv6 transfer failed: got '$RESULT'"
fi
cleanup

# ── Test 7: File to stdout ────────────────────────────────────────────────────
echo ""
echo "Test 7: File to stdout"
echo "stdout test" > /tmp/rdt_input.bin
$BINARY -s -p $PORT -w $TIMEOUT > /tmp/rdt_received.bin &
SERVER_PID=$!
sleep 0.2
$BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
wait $SERVER_PID
RESULT=$(cat /tmp/rdt_received.bin)
if [ "$RESULT" = "stdout test" ]; then
    pass "stdout output ok"
else
    fail "stdout output failed: got '$RESULT'"
fi
cleanup

# ── Test 8: Client timeout (no server) ───────────────────────────────────────
echo ""
echo "Test 8: Client timeout when no server"
echo "test" | $BINARY -c -a 127.0.0.1 -p $PORT -w 1
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    pass "client exited with non-zero code ($EXIT_CODE)"
else
    fail "client should have failed but returned 0"
fi
cleanup

# ── Test 9: Server timeout (no client) ───────────────────────────────────────
echo ""
echo "Test 9: Server timeout when no client"
$BINARY -s -p $PORT -o /tmp/rdt_received.bin -w 1
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then
    pass "server exited with non-zero code ($EXIT_CODE)"
else
    fail "server should have failed but returned 0"
fi
cleanup

# ── Test 10: Packet loss simulation ──────────────────────────────────────────
echo ""
echo "Test 10: Packet loss simulation (10% loss via tc netem)"
if ! command -v tc &>/dev/null; then
    echo "  [SKIP] tc not available"
else
    # Add 10% packet loss on loopback
    sudo tc qdisc add dev lo root netem loss 10% 2>/dev/null
    dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=512 2>/dev/null
    start_server
    $BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
    wait $SERVER_PID
    # Remove netem
    sudo tc qdisc del dev lo root 2>/dev/null
    IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
    OUT_MD5=$(md5sum /tmp/rdt_received.bin | cut -d' ' -f1)
    if [ "$IN_MD5" = "$OUT_MD5" ]; then
        pass "transfer correct despite 10% packet loss"
    else
        fail "data corrupted under packet loss"
    fi
fi
cleanup

# ── Test 11: Packet reorder simulation ───────────────────────────────────────
echo ""
echo "Test 11: Packet reorder simulation (via tc netem)"
if ! command -v tc &>/dev/null; then
    echo "  [SKIP] tc not available"
else
    sudo tc qdisc add dev lo root netem delay 10ms reorder 50% 2>/dev/null
    dd if=/dev/urandom of=/tmp/rdt_input.bin bs=1024 count=512 2>/dev/null
    start_server
    $BINARY -c -a 127.0.0.1 -p $PORT -i /tmp/rdt_input.bin -w $TIMEOUT
    wait $SERVER_PID
    sudo tc qdisc del dev lo root 2>/dev/null
    IN_MD5=$(md5sum /tmp/rdt_input.bin | cut -d' ' -f1)
    OUT_MD5=$(md5sum /tmp/rdt_received.bin | cut -d' ' -f1)
    if [ "$IN_MD5" = "$OUT_MD5" ]; then
        pass "transfer correct despite reordering"
    else
        fail "data corrupted under reordering"
    fi
fi
cleanup

# ── Results ───────────────────────────────────────────────────────────────────
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ $FAIL -eq 0 ] && exit 0 || exit 1