# source it to configure for compile and execution
#
if [ -z "$SIMPATH" ]; then
  echo "SIMPATH not defined"
  return 1
fi

if [ -z "$CBMNETPATH" ]; then
export CBMNETPATH=$PWD
#
export LD_LIBRARY_PATH=$CBMNETPATH/lib:$SIMPATH/lib:$LD_LIBRARY_PATH
fi
