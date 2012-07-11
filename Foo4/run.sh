#!/bin/sh

if [ $# -ne 3 ]
then
  echo "Usage: `basename $0` <file>.c <event_handler> <reverse_handler_to_emit>"
  exit $E_BADARGS
fi

# Army directory layout
DIRECTORY="/Users/jlapre/temp/llvm-mac-install/bin"

if [ -d "$DIRECTORY" ]; then
#    echo "$DIRECTORY found"
    export PATH="$DIRECTORY":$PATH
    LIB="/Users/jlapre/temp/llvm-mac-install/lib/LLVMFoo4.dylib"
#    echo $PATH
fi

DIRECTORY="/Users/jlapre/temp/llvm-gcc/bin"

if [ -d "$DIRECTORY" ]; then
#    echo "$DIRECTORY found"
    export PATH="$DIRECTORY":$PATH
#    echo $PATH
fi

# odin directory layout
DIRECTORY="/Users/laprej/temp/llvm-3.0-install/bin"

if [ -d "$DIRECTORY" ]; then
#    echo "$DIRECTORY found"
    export PATH="$DIRECTORY":$PATH
    LIB="/Users/laprej/temp/llvm-3.0-install/lib/LLVMFoo4.dylib"
#    echo $PATH
fi

DIRECTORY="/Users/laprej/temp/llvm-gcc/bin"

if [ -d "$DIRECTORY" ]; then
#    echo "$DIRECTORY found"
    export PATH="$DIRECTORY":$PATH
#    echo $PATH
fi

if [ ! -f "$1.c" ]
then
    echo "$1.c not found!"
    exit
fi

llvm-gcc -emit-llvm $1.c -c -o $1.bc

llvm-gcc -emit-llvm $1.c -S -o $1.ll

echo "opt $DBG -break-crit-edges -loop-simplify -indvars -disable-verify -load "$LIB" -hello -rev-func=$2 -tgt-func=$3 < $1.bc > $1-output.bc"

opt $DBG -break-crit-edges -loop-simplify -indvars -disable-verify -load "$LIB" -hello -rev-func=$2 -tgt-func=$3 < $1.bc > $1-output.bc

echo "opt completed."

llc $1-output.bc

echo "llc completed."

llvm-dis $1-output.bc

echo "llvm-dis completed."

#gcc test.cbe.c

clang -c $1-output.s

echo "clang completed."

exit
