#!/bin/sh

export PATH=/Users/jlapre/temp/llvm-mac-install/bin:/Users/jlapre/temp/llvm-gcc/bin:$PATH

llvm-gcc -emit-llvm hello.c -c -o hello.bc

llvm-gcc -emit-llvm hello.c -S -o hello.ll

opt -debug -load /Users/jlapre/temp/llvm-mac-install/lib/LLVMFoo4.dylib -hello -rev-func=foobar -tgt-func=barbar < hello.bc > test.bc

llc -march=c test.bc

llvm-dis test.bc

gcc test.cbe.c

exit
