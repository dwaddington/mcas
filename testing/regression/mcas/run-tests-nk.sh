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

DEVDAX_PFX="$(find_devdax)"
if test -n "$DEVDAX_PFX"
then declare -a devices=($(ls -l ${DEVDAX_PFX}.* | awk '{gsub(",",":") ; print $5 $6}'))
  raw_store_size=$(cat "/sys/dev/char/${devices[0]})/size")
  echo "DAX_PREFIX=${$DEVDAX_PFX} RAW_STORE_SIZE=$raw_store_size run_hstore has_module_mcasmod $1"
  DAX_PREFIX="$DEVDAX_PFX" RAW_STORE_SIZE=$raw_store_size run_hstore has_module_mcasmod $1
fi

FSDAX_DIR="$(find_fsdax)"
if test -n "$FSDAX_DIR"
then raw_store_size=$(($(df /mnt/pmem1 --output=avail | tail -1)*1024))
  echo "DAX_PREFIX=${FSDAX_DIR} USE_ODP=1 RAW_STORE_SIZE=$raw_store_size run_hstore true $1"
  DAX_PREFIX="$FSDAX_DIR" USE_ODP=1 RAW_STORE_SIZE=$raw_store_size run_hstore true $1
fi

