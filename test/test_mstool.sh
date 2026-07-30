#!/bin/bash

set -o errexit
set -o pipefail

# archive -> archive
./mstool -i test/example2.msa -o test/example2.mstool.msa

# pattern generator -> archive
./mstool -p 0 -n 10 -o test/pgen.mstool.msa

N=`./mstool -i test/example2.mstool.msa -a 2>&1 | grep total | sed -e 's/.* //'`
echo "microslices in output file: $N"

if [ "$N" -ne 4 ]; then
	echo "not ok"
	exit 1
fi

N=`./mstool -i test/pgen.mstool.msa -a 2>&1 | grep total | sed -e 's/.* //'`
echo "microslices in pattern generator output file: $N"

if [ "$N" -ne 10 ]; then
	echo "not ok"
	exit 1
else
	echo "ok"
	exit 0
fi
