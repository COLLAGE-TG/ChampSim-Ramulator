#!/bin/sh

#PINを用いてトレースを取得するだけ。

SAMPLE_FILENAME=$1
WARMUP=$2
SAMPLE_INST=$3
FILE_NAME="${SAMPLE_FILENAME}_${WARMUP}_${SAMPLE_INST}.champsim"
GC_ROOT=/home/funkytaiga/tools/gc-8.2.2/

$PIN_ROOT/pin -t /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tracer/pin/obj-intel64/champsim_tracer.so -o /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_trace/${FILE_NAME} -s ${WARMUP} -t ${SAMPLE_INST} -- /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_benchmark/./a.out 