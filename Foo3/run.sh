#!/bin/sh

export PATH=/Users/laprej/temp/llvm-install/bin:/Users/laprej/temp/llvm-gcc/bin:$PATH

llvm-gcc -emit-llvm hello.c -c -o hello.bc

llvm-gcc -emit-llvm hello.c -S -o hello.ll

opt -debug -load /Users/laprej/temp/llvm-mac-install/lib/LLVMFoo3.dylib -hello -rev-func=foobar -tgt-func=barbar < hello.bc > test.bc

llc -march=c test.bc

llvm-dis test.bc

gcc test.cbe.c

exit
