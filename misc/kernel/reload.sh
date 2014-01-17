#!/bin/bash
BASE=build/`uname -r`
MODULE=rorcfs

if lsmod | grep $MODULE 2>&1 >/dev/null; then
	echo "Unloading module $MODULE"
	rmmod -f $MODULE
fi
echo "Inserting module $MODULE"
insmod $BASE/$MODULE.ko

./gdb-add-symbol.sh $MODULE $BASE/$MODULE.o
