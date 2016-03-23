#!/bin/bash

set -o errexit
set -o pipefail

./mstool -i test/example2.msa -O test_mstool_1 &
inst1_pid=$!
sleep 0.5

./mstool -I test_mstool_1 -O test_mstool_2 &
inst2_pid=$!
sleep 0.5

./mstool -I test_mstool_2 -o test/example2.mstool.msa &
inst3_pid=$!

echo "waiting for instance 1..."
wait $inst1_pid
echo "waiting for instance 2..."
wait $inst2_pid
echo "waiting for instance 3..."
wait $inst3_pid

N=`./mstool -i test/example2.mstool.msa -a 2>&1 | grep total | sed -e 's/.* //'`
echo "microslices in output file: $N"

if [ "$N" -ne 4 ]; then
	echo "not ok"
	exit 1
else
	echo "ok"
	exit 0
fi
