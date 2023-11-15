#!/bin/sh

# c言語のプログラミングをコンパイルして、実行する。
# その際GCが行われるようにする。
# 実行例
# bash gc_exe_c.sh {filename}

# GC_ROOT=/home/funkytaiga/tools/gc-8.2.2
GC_ROOT=/home/funkytaiga/tools
# GC_ROOT=/home/funkytaiga/ChampSim_Ramulator
gcc -I${GC_ROOT}/include -L${GC_ROOT}/gc-8.2.2/.libs -Wl,-R${GC_ROOT}/gc-8.2.2/.libs ${1} -lgc -g -O0
/usr/bin/time ./a.out