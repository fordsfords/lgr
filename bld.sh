#!/bin/sh

CACHE_LINE_SIZE=64 # For "q.c" on x86, sparc

OPTS=""
if echo "$OSTYPE" | egrep -i linux >/dev/null; then :
  OPTS="-l rt -l pthread"
fi

egrep "\?\?\?" *.c *.h

gcc -Wall -g -O3 -DCACHE_LINE_SIZE=$CACHE_LINE_SIZE -o lgr_test $OPTS lgr_hook.c q.c cprt.c lgr_test.c
if [ $? -ne 0 ]; then exit 1; fi

gcc -Wall -g -O3 -DCACHE_LINE_SIZE=$CACHE_LINE_SIZE -o lgr_perf $OPTS lgr.c q.c cprt.c lgr_perf.c
if [ $? -ne 0 ]; then exit 1; fi
