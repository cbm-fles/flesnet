#!/bin/bash

out=$(mktemp)
err=$(mktemp)

trap 'rm -f $out $err' EXIT

./tsa2msa --version > $out 2> $err
if [ $? -ne 0 ]; then
  echo "Trying to get version failed"
  exit 1
else
  if ! grep -q "tsa2msa" $out; then
    echo "Version output does not contain 'tsa2msa'"
	exit 1
  fi
  if [ -s $err ]; then
	echo "stderr of version command is not empty"
	exit 1
  fi
fi

./tsa2msa --version --help > $out 2> $err
if [ $? -eq 0 ]; then
  echo "Trying to get version with help did not fail"
  exit 1
else
  # There is a parsing error as help is not supposed to be used with
  # version command. Output should be empty and error should contain
  # usage and error message
  if [ -s $out ]; then
	echo "stdout of version command with help is not empty"
	cat $out
	exit 1
  fi

  if ! [ -s $err ] ; then
	echo "stderr of version command with help is empty"
	exit 1
  fi
  if ! grep -q "tsa2msa" $err; then
	echo "Version output does not contain 'tsa2msa'"
  fi
  if ! grep -q "Error" $err; then
	echo "stderr of version command with help does not contain 'Error'"
	exit 1
  fi
fi


