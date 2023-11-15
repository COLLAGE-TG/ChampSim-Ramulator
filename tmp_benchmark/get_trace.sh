#!/bin/sh

#コンパイルする
#その後PINを用いてトレースを取得する。

SAMPLE_FILENAME=$1
WARMUP=$2
SAMPLE_INST=$3
DO_GC=${4:-1} #bool型 0ならnot GC, 1ならGCする。デフォルトは1。
FILE_NAME="${SAMPLE_FILENAME}_${WARMUP}_${SAMPLE_INST}.champsim"
GC_ROOT=/home/funkytaiga/tools/gc-8.2.4/

if [ $DO_GC -eq 1 ]; then
  gcc -I${GC_ROOT}/include -L${GC_ROOT}/lib -Wl,-R${GC_ROOT}/lib ${1} -lgc
elif [ $DO_GC -eq 0 ]; then
  gcc ${1}
else
  echo "DO_GCの引数は0または1です。"
  exit 1
fi

/usr/bin/time ./a.out
$PIN_ROOT/pin -t /home/funkytaiga/ChampSim-Ramulator/tracer/pin/obj-intel64/champsim_tracer.so -o /home/funkytaiga/ChampSim-Ramulator/tmp_trace/${FILE_NAME} -s ${WARMUP} -t ${SAMPLE_INST} -- /home/funkytaiga/ChampSim-Ramulator/tmp_benchmark/./a.out 