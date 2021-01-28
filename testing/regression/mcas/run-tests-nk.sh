#!/bin/bash

# Tests of various size reads, writes, and 50% mixes
# (Store size queried and passed to the test cases,
# to avoid asking for more space than is available.)

ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"
DELAY=8

run_hstore() {
  typeset ado_prereq="$1"
  shift
  # run each test
  for vl in 512 960 1024 4000 4096 16384 32768 65536
  do for rd_pct in 0 50 100
    do echo VALUE_LENGTH=$vl READ_PCT=$rd_pct $DIR/hstore-throughput.sh $1
      VALUE_LENGTH=$vl READ_PCT=$rd_pct $DIR/hstore-throughput.sh $1
      sleep $DELAY
    done
  done
}

if has_devdax
then raw_store_size=$(cat "/sys/dev/char/$(ls -l /dev/dax0.0 | awk '{gsub(",",":") ; print $5 $6}')/size")
  echo "DAXTYPE=devdax RAW_STORE_SIZE=$raw_store_size run_hstore has_module_mcasmod $1"
  DAXTYPE=devdax RAW_STORE_SIZE=$raw_store_size run_hstore has_module_mcasmod $1
fi

if has_fsdax
then raw_store_size=$(($(df /mnt/pmem1 --output=avail | tail -1)*1024))
  echo "DAXTYPE=fsdax USE_ODP=1 RAW_STORE_SIZE=$raw_store_size run_hstore true $1"
  DAXTYPE=fsdax USE_ODP=1 RAW_STORE_SIZE=$raw_store_size run_hstore true $1
fi

