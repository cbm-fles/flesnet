#!/bin/bash
# Build timeslices from microslice archives with tsclient (msa scheme)

set -o errexit
set -o pipefail

OUT="test/msa_build_$$"
cleanup() {
	rm -f "$OUT"*
}
trap cleanup EXIT

# three links with 1 ms microslices from 0 to 499 ms
for i in 0 1 2; do
	./mstool -p $i -n 500 --microslice-duration 1ms -o "${OUT}_link$i.msa" -l warning
done

# 10 ms timeslices: 1 to 48 can be built (0 lacks the overlap before,
# 49 the microslice beyond its end)
./tsclient -i "msa://${OUT}_link*.msa?duration=10ms&overlap=100us" -a -o "file://${OUT}.tsa" -l warning
N=`./tsclient -i "${OUT}.tsa" -a 2>&1 | grep total | sed -e 's/.* //'`
echo "timeslices built: $N"
if [ "$N" -ne 48 ]; then
	echo "not ok"
	exit 1
fi
echo "ok"
exit 0
