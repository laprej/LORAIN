#!/bin/sh

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

llvm-gcc -emit-llvm hello.c -c -o hello.bc

llvm-gcc -emit-llvm hello.c -S -o hello.ll

echo "opt -disable-verify -debug -load "$LIB" -hello -rev-func=foobar -tgt-func=barbar < hello.bc > test.bc"

opt -break-crit-edges -disable-verify -debug -load "$LIB" -hello -rev-func=foobar -tgt-func=barbar < hello.bc > test.bc

echo "opt completed."

llc test.bc

llvm-dis test.bc

#gcc test.cbe.c

clang test.s

exit
