#!/bin/bash

set -e
DIR="$(git rev-parse --show-toplevel)"

timeout 60 $DIR/build/readout config $DIR/contrib/readout_eda02.conf
grep -q -e "INFO: total timeslices processed: 1000" tsclient.log

echo "TEST PASSED"
