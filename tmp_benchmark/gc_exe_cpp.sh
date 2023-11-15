#!/bin/sh

# c++言語のプログラミングをコンパイルして、実行する。
# その際GCが行われるようにする。
# 実行例
# bash gc_exe_cpp.sh {filename}

GC_ROOT=/home/funkytaiga/tools/gc-8.2.4/

g++ -I${GC_ROOT}/include -L${GC_ROOT}/lib -Wl,-R${GC_ROOT}/lib -DGC_OPERATOR_NEW_ARRAY ${1} -lgc
/usr/bin/time ./a.out
