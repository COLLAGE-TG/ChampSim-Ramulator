#!/bin/sh

#コンパイルする
#その後PINを用いてトレースを取得する。

SAMPLE_FILENAME=$1
WARMUP=$2
SAMPLE_INST=$3
DO_GC=${4:-1} #bool型 0ならnot GC, 1ならGCする。デフォルトは1。
GC_ROOT=/home/funkytaiga/tools/gc-8.2.2/
# SAMPLE_FILENAMEの.以下を削除して新しい変数NEW_FILENAMEに格納
SAMPLE_FILENAME_NO_DOT="${SAMPLE_FILENAME%.*}"
FILE_NAME="${SAMPLE_FILENAME_NO_DOT}_${WARMUP}_${SAMPLE_INST}.champsim"

if [ $DO_GC -eq 1 ]; then
  gcc -I${GC_ROOT}/include -L${GC_ROOT}/.libs -Wl,-R${GC_ROOT}/lib ${1} -lgc -o ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out
elif [ $DO_GC -eq 0 ]; then
  gcc ${SAMPLE_FILENAME}
else
  echo "DO_GCの引数は0または1です。"
  exit 1
fi

echo "/usr/bin/time ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out"
/usr/bin/time ./ex_files/${SAMPLE_FILENAME_NO_DOT}.out
echo "$PIN_ROOT/pin -t /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tracer/pin/obj-intel64/champsim_tracer.so -o /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_trace/${FILE_NAME} -s ${WARMUP} -t ${SAMPLE_INST} -- /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_benchmark/ex_files/./${SAMPLE_FILENAME_NO_DOT}.out"
$PIN_ROOT/pin -t /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tracer/pin/obj-intel64/champsim_tracer.so -o /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_trace/${FILE_NAME} -s ${WARMUP} -t ${SAMPLE_INST} -- /home/funkytaiga/tmp_champ/ChampSim-Ramulator/tmp_benchmark/ex_files/./${SAMPLE_FILENAME_NO_DOT}.out
xz -f ../tmp_trace/${SAMPLE_FILENAME_NO_DOT}_${WARMUP}_${SAMPLE_INST}.champsim