#!/bin/sh

CACHE_LINE_SIZE=64 # x86, sparc

OPTS=""
if echo "$OSTYPE" | egrep -i linux >/dev/null; then :
  OPTS="-l rt -l pthread"
fi

gcc -Wall -g -O3 -DCACHE_LINE_SIZE=$CACHE_LINE_SIZE -o lgr_test $OPTS cprt.c q.c lgr.c lgr_test.c
if [ $? -ne 0 ]; then exit 1; fi
