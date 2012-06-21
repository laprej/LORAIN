#!/bin/sh

if [ $# -ne 1 ]
then
    echo "usage: `basename $0` <ll_file>"
    exit $E_BADARGS
fi

if [ ! -f "$1" ]
then
    echo "$1 not found!"
    exit
fi

#odin
if [ -d /Users/laprej/temp/llvm-3.0-install/bin ]; then
    export PATH=/Users/laprej/temp/llvm-3.0-install/bin:$PATH
fi
#army
if [ -d /Users/jlapre/temp/llvm-mac-install/bin ]; then
    export PATH=/Users/jlapre/temp/llvm-mac-install/bin:$PATH
fi

llvm-as < $1 | opt -analyze -view-cfg
