#!/bin/sh

# c言語のプログラミングをコンパイルして、実行する。
# その際GCが行われるようにする。
# 実行例
# bash gc_exe_c.sh {filename}
SAMPLE_FILENAME=$1
SAMPLE_FILENAME_NO_DOT="${SAMPLE_FILENAME%.*}"
# GC_ROOT=/home/funkytaiga/tools/gc-8.2.2
GC_ROOT=/home/funkytaiga/tools
# GC_ROOT=/home/funkytaiga/ChampSim_Ramulator
echo "gcc -I${GC_ROOT}/gc-8.2.2/include -L${GC_ROOT}/gc-8.2.2/.libs -Wl,-R${GC_ROOT}/gc-8.2.2/lib ${SAMPLE_FILENAME} -lgc -g -o ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out" 
gcc -I${GC_ROOT}/gc-8.2.2/include -L${GC_ROOT}/gc-8.2.2/.libs -Wl,-R${GC_ROOT}/gc-8.2.2/lib ${SAMPLE_FILENAME} -lgc -g -o ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out
echo "/usr/bin/time ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out"
/usr/bin/time ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out