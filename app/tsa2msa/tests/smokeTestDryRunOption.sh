#!/bin/bash

# Check that the tsa2msa binary is in the current directory and executable:
if [ ! -x tsa2msa ]; then
  >&2 echo "$0 Error: tsa2msa binary not found or not executable"
  exit 1
fi

example_tsa="test/reference/example1.tsa"

# Check that $example_tsa is found and readable:
if [ ! -r $example_tsa ]; then
  >&2 echo "$0 Error: $example_tsa file not found or not readable"
  exit 1
fi

old_pwd=$(pwd)


tmpdir=$(mktemp -d)
trap "rm -rf $tmpdir" EXIT
cp $example_tsa $tmpdir
cd $tmpdir

"$old_pwd"/tsa2msa --dry-run example1.tsa
exit_code=$?
if [ "$exit_code" -ne 0 ]; then
  >&2 echo "$0 Error: tsa2msa --dry-run failed with exit code $exit_code"
  exit 1
fi

rm example1.tsa

# Check if current directory is empty:
# (From an answer to https://superuser.com/questions/352289/bash-scripting-test-for-empty-directory)
if [ -z "$(find . -maxdepth 0 -type d -empty 2>/dev/null)" ]; then
  >&2 echo "$0 Error: current directory is not empty"
  exit 1
fi



