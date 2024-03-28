#!/bin/bash

out=$(mktemp)
err=$(mktemp)

trap 'rm -f $out $err' EXIT

./tsa2msa --help > $out 2> $err
if [ $? -ne 0 ]; then
  echo "Trying to get help failed"
  exit 1
else
  if ! grep -q "tsa2msa" $out; then
	echo "Help output does not contain 'tsa2msa'"
	exit 1
  fi
  if [ -s $err ]; then
	echo "stderr of help command is not empty"
	cat $err
	exit 1
  fi
fi

./tsa2msa --help --quiet > $out 2> $err
if [ $? -eq 0 ] ; then
  echo "Trying to get help with quiet did not fail"
  exit 1
else
  # There is a parsing error as quiet is not supposed to be used with
  # help command. Output should be empty and error should contain
  # usage and error message
  if [ -s $out ]; then
	echo "stdout of help command with quiet is not empty"
	cat $out
	exit 1
  fi

  if ! [ -s $err ] ; then
	echo "stderr of help command with quiet is empty"
	exit 1
  fi
  if ! grep -q "tsa2msa" $err; then
	echo "Help output does not contain 'tsa2msa'"
  fi
  if ! grep -q "Error" $err; then
	echo "stderr of help command with quiet does not contain 'Error'"
	exit 1
  fi
fi

./tsa2msa --help --verbose > $out 2> $err
if [ $? -ne 0 ]; then
  echo "Trying to get help with verbose failed"
  exit 1
else
  if ! grep -q "tsa2msa" $out; then
	echo "Help output does not contain 'tsa2msa'"
	exit 1
  fi
  if ! grep -q "Hidden" $out; then
	echo "Help output does not contain hidden options"
	exit 1
  fi
  if ! grep -q "input" $out; then
	echo "Help output does not contain hidden input option"
	exit 1
  fi

  if [ -s $err ]; then
	echo "stderr of help command with verbose is not empty"
	cat $err
	exit 1
  fi
fi



