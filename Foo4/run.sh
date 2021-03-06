#!/bin/sh

if [ $# -ne 3 -a $# -ne 4 ]
then
  echo "Usage: `basename $0` <file>.c <event_handler> <reverse_handler_to_emit> [1 (if you're using a pre-generated BC file)]"
  exit $E_BADARGS
fi

# mastiff directory layout
DIRECTORY="/home/laprej/temp/clang+llvm-3.2-x86_64-linux-ubuntu-12.04/bin"

if [ -d "$DIRECTORY" ]; then
    export PATH="$DIRECTORY":$PATH
fi

DIRECTORY="/home/laprej/temp/llvm-3.2-install/bin"

if [ -d "$DIRECTORY" ]; then
    export PATH="$DIRECTORY":$PATH
    LIB="/home/laprej/temp/llvm-3.2-install/lib/LLVMFoo4.so"
    SCLIB="/home/laprej/temp/llvm-3.2-install/lib/BCGEPs.so"
    AUGLIB="/home/laprej/temp/llvm-3.2-install/lib/aug-struct.so"
fi

# odin directory layout
DIRECTORY="/Users/laprej/temp/clang+llvm-3.2-x86_64-apple-darwin11/bin"

if [ -d "$DIRECTORY" ]; then
    export PATH="$DIRECTORY":$PATH
fi

DIRECTORY="/Users/laprej/temp/llvm-3.2-install/bin"

if [ -d "$DIRECTORY" ]; then
    export PATH="$DIRECTORY":$PATH
    LIB="/Users/laprej/temp/llvm-3.2-install/lib/LLVMFoo4.dylib"
    SCLIB="/Users/laprej/temp/llvm-3.2-install/lib/BCGEPs.dylib"
    AUGLIB="/Users/laprej/temp/llvm-3.2-install/lib/aug-struct.dylib"
fi

if [ ! -f "$1.c" -a $# -eq 3 ]
then
    echo "$1.c not found!"
    exit
fi

if [ $# -eq 3 ]
then
    echo clang ${CFLAGS} -emit-llvm $1.c -c -o $1.bc
    clang ${CFLAGS} -emit-llvm $1.c -c -o $1.bc
fi

echo "opt $DBG -always-inline -stats -load $SCLIB -load $AUGLIB -break-constgeps -lowerswitch -mergereturn -reg2mem -aug-struct -ins-func=$2 -update-func=$3 $1.bc -o $1-nogeps.bc"
opt $DBG -always-inline -stats -load $SCLIB -load $AUGLIB -break-constgeps -lowerswitch -mergereturn -reg2mem -aug-struct -ins-func=$2 -update-func=$3 $1.bc -o $1-nogeps.bc

echo "mv $1-nogeps.bc $1.bc"
mv $1-nogeps.bc $1.bc

llvm-dis $1.bc

echo "opt $DBG -load "$LIB" -loop-simplify -indvars -hello -rev-func=$2 -tgt-func=$3 $1.bc -o $1-output.bc"
opt $DBG -load "$LIB" -loop-simplify -indvars -hello -rev-func=$2 -tgt-func=$3 $1.bc -o $1-output.bc

echo "opt completed."

llc $1-output.bc

echo "llc completed."

llvm-dis $1-output.bc

echo "llvm-dis completed."

#gcc test.cbe.c

clang -c $1-output.s

echo "clang completed."

exit
