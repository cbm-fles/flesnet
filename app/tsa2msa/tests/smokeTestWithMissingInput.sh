#!/bin/bash

out=$(mktemp)
err=$(mktemp)

trap 'rm -f $out $err' EXIT

./tsa2msa > $out 2> $err
if [ $? -eq 0 ]; then
  echo "Running without input did not fail"
  exit 1
else
  if ! [ -s $err ]; then
	echo "stderr of command without input is empty"
	exit 1
  fi
  if ! grep -q "Error" $err; then
	echo "stderr of command without input does not contain 'Error'"
	exit 1
  fi
fi

exit 0
