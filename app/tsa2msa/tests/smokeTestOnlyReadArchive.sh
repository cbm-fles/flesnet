#!/bin/bash

# Check that the tsa2msa binary is in the current directory and executable:
if [ ! -x tsa2msa ]; then
  >&2 echo "$0 Error: tsa2msa binary not found or not executable"
  exit 1
fi

example_tsa="test/reference/example1.tsa"

# Check that $example_tsa is found and readable:
if [ ! -r $example_tsa ]; then
  >&2 echo "$0 Error: example_tsa file not found or not readable"
  exit 1
fi

./tsa2msa --dry-run example1.tsa
if [ $? -ne 0 ]; then
  >&2 echo "$0 Error: tsa2msa --dry-run failed"
  exit 1
fi


