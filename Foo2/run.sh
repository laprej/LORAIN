#!/bin/sh

export PATH=/Users/laprej/llvm-install/bin:/Users/laprej/llvm-gcc/bin:$PATH

llvm-gcc -emit-llvm hello.c -c -o hello.bc

llvm-gcc -emit-llvm hello.c -S -o hello.ll

opt -debug -load /Users/laprej/llvm-bin/lib/LLVMFoo2.so -foo -rev-func=foobar -tgt-func=barbar < hello.bc > test.bc

llc -march=c test.bc

llvm-dis test.bc

gcc test.cbe.c

exit
