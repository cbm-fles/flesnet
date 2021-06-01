#!/bin/bash

OUT=$(df -h -BG --output=target,avail,pcent | grep local)
# Testing
#echo "$OUT"
#[[ $OUT =~ /local\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ 379G\ \ ([[:digit:]]+)\% ]]
#[[ $OUT =~ /local\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ([[:digit:]]+)([[:alpha:]])\ \ 80\% ]]

# Formatting with xxxG
[[ $OUT =~ /local\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ([[:digit:]]+)([[:alpha:]])\ \ ([[:digit:]]+)\% ]]
if [ -z ${BASH_REMATCH[1]} ] &&  [ -z ${BASH_REMATCH[1]} ]; then
   # Formatting with xxxxG
   [[ $OUT =~ /local\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ([[:digit:]]+)([[:alpha:]])\ \ ([[:digit:]]+)\% ]]
fi
if [ -z ${BASH_REMATCH[1]} ] &&  [ -z ${BASH_REMATCH[1]} ]; then
   # Formatting with xxG
   [[ $OUT =~ /local\ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ \ ([[:digit:]]+)([[:alpha:]])\ \ ([[:digit:]]+)\% ]]
fi
echo "On ${SLURMD_NODENAME} Left ${BASH_REMATCH[1]}${BASH_REMATCH[2]} usage ${BASH_REMATCH[3]}%"
