#!/bin/bash

# End-to-end test of the shared memory online interface: one tsclient serves a
# timeslice buffer, others consume it through the item protocol. Uses only
# tsclient, so it needs neither PDA nor UCX.

set -o errexit
set -o pipefail

ID="test_shm_stream_$$"
OUT="test/shm_stream_$$"
PIDS=""

cleanup() {
	for pid in $PIDS; do
		kill "$pid" 2>/dev/null || true
	done
	rm -f "$OUT"*.tsa
}
trap cleanup EXIT

count_timeslices() {
	./tsclient -i "$1" -a 2>&1 | grep total | sed -e 's/.* //'
}

# The reference archive holds 2 timeslices of 2 components each.
INPUT='test/example1.tsa?cycles=20'
EXPECTED=40

# A consumer has to be registered before the producer starts, otherwise the
# items are gone before anyone asks for them.
echo "== end of stream terminates the consumer =="
timeout -k 5 60 ./tsclient -i "shm://$ID" -o "file://$OUT.tsa" &
PIDS="$!"
sleep 0.5
timeout -k 5 60 ./tsclient -i "$INPUT" -o "shm://127.0.0.1/$ID?size=16MiB"

# No -n was given, so the consumer can only have exited on end of stream.
wait $PIDS
PIDS=""

N=$(count_timeslices "$OUT.tsa")
echo "timeslices received: $N"
if [ "$N" -ne "$EXPECTED" ]; then
	echo "not ok: expected $EXPECTED"
	exit 1
fi

echo "== stride and offset split the stream =="
ID="${ID}_b"
timeout -k 5 60 ./tsclient -i "shm://$ID?stride=2&offset=0" -o "file://$OUT.even.tsa" &
PIDS="$!"
timeout -k 5 60 ./tsclient -i "shm://$ID?stride=2&offset=1" -o "file://$OUT.odd.tsa" &
PIDS="$PIDS $!"
sleep 0.5
timeout -k 5 60 ./tsclient -i "$INPUT" -o "shm://127.0.0.1/$ID?size=16MiB"

for pid in $PIDS; do
	wait "$pid"
done
PIDS=""

EVEN=$(count_timeslices "$OUT.even.tsa")
ODD=$(count_timeslices "$OUT.odd.tsa")
echo "timeslices received: even $EVEN, odd $ODD"
if [ "$EVEN" -ne $((EXPECTED / 2)) ] || [ "$ODD" -ne $((EXPECTED / 2)) ]; then
	echo "not ok: expected $((EXPECTED / 2)) each"
	exit 1
fi

echo "ok"
exit 0
