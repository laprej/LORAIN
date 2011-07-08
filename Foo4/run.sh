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
DIRECTORY="/Users/laprej/temp/llvm-mac-install/bin"

if [ -d "$DIRECTORY" ]; then
#    echo "$DIRECTORY found"
    export PATH="$DIRECTORY":$PATH
    LIB="/Users/laprej/temp/llvm-mac-install/lib/LLVMFoo4.dylib"
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

opt -debug -load "$LIB" -hello -rev-func=foobar -tgt-func=barbar < hello.bc > test.bc

llc -march=c test.bc

llvm-dis test.bc

gcc test.cbe.c

exit
