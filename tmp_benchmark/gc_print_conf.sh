#!/bin/sh
#
# config of printing log
#
#
if [ $1 = "print" ]
then
	export GC_PRINT_STATS=1
elif [ $1 = "verbose" ]
then
	export GC_PRINT_VERBOSE_STATS=1
elif [ $1 = "map" ]
then
	GC_PRINT_ADDRESS_MAP=1
elif [ $1 = "clear" ]
then
	export GC_PRINT_STATS=0
	export GC_PRINT_VERBOSE_STATS=0
	export GC_PRINT_ADDRESS_MAP=0
else
	echo "ERROR :correct argument required"
fi

echo "GC_PRINT_STATS = $GC_PRINT_STATS"
echo "GC_PRINT_VERBOSE_STATS = $GC_PRINT_VERBOSE_STATS"
echo "GC_PRINT_ADDRESS_MAP = $GC_PRINT_ADDRESS_MAP"

