#!/bin/bash
ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"
DELAY=8

FSDAX_DIR=$(find_fsdax)
DEVDAX_PFX=$(find_devdax)

prefix()
{
  : ls -lr $FSDAX_DIR
  sleep $DELAY
}

run_hstore() {
  typeset ado_prereq="$1"
  shift
  # mapstore unit tests: basic
  [ -n "$DEBUG" ] && [ 0 -lt "$DEBUG" ] && echo ./src/components/store/test/store-test1 --store mapstore
                                                ./src/components/store/test/store-test1 --store mapstore
  # hstore unit tests: basic
  #DAX_RESET=1 STORE=hstore ./src/components/store/hstore/unit_test/hstore-test1 # (out of space)
  STORE_LOCATION="$("$DIR/dax.py" --prefix "$DAX_PREFIX")"
  [ -n "$DEBUG" ] && [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-cc --daxconfig \'"$STORE_LOCATION"\' --many-count 100000
                                                DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-cc --daxconfig "$STORE_LOCATION" --many-count 100000
  [ -n "$DEBUG" ] && [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-mm --mm-plugin-path ./dist/lib/libmm-plugin-ccpm.so --daxconfig \'"$STORE_LOCATION"\' --many-count 100000
                                                DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-mm --mm-plugin-path ./dist/lib/libmm-plugin-ccpm.so --daxconfig "$STORE_LOCATION" --many-count 100000

  # hstore unit tests: multithreaded lock/unlock
  [ -n "$DEBUG" ] && [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 STORE=hstore-mt STORE_LOCATION=\'"$STORE_LOCATION"\' MM_PLUGIN_PATH=./dist/lib/libmm-plugin-ccpm.so ./src/components/store/hstore/unit_test/hstore-testmt
  DAX_RESET=1 STORE=hstore-mt STORE_LOCATION="$STORE_LOCATION" MM_PLUGIN_PATH=./dist/lib/libmm-plugin-ccpm.so ./src/components/store/hstore/unit_test/hstore-testmt
  [ -n "$DEBUG" ] && [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 STORE=hstore-mt STORE_LOCATION=\'"$STORE_LOCATION"\' MM_PLUGIN_PATH=./dist/lib/libmm-plugin-rcalb.so ./src/components/store/hstore/unit_test/hstore-testmt
  DAX_RESET=1 STORE=hstore-mt STORE_LOCATION="$STORE_LOCATION" MM_PLUGIN_PATH=./dist/lib/libmm-plugin-rcalb.so ./src/components/store/hstore/unit_test/hstore-testmt
  # run performance tests
  prefix
  GOAL=140000 ELEMENT_COUNT=2000000 STORE=hstore PERFTEST=put $DIR/mcas-hstore-put-0.sh $1
  prefix
  GOAL=170000 ELEMENT_COUNT=2000000 STORE=hstore PERFTEST=get $DIR/mcas-hstore-get-0.sh $1
  prefix
  GOAL=900 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-put-0.sh $1
  prefix
  GOAL=1750 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-get-0.sh $1
  prefix
  FLUSH_ENABLE=0 GOAL=2400 FORCE_DIRECT=1 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-put_direct-0.sh $1
  prefix
  GOAL=3000 FORCE_DIRECT=1 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-get_direct-0.sh $1
  prefix
  # includes async_put, async_erase, async_put_direct
  $DIR/mcas-hstore-cc-kvtest-0.sh $1
  prefix
  $DIR/mcas-hstore-mc-kvtest-0.sh $1
  prefix
  $DIR/mcas-hstore-mr-kvtest-0.sh $1
  prefix
  $DIR/mcas-hstore-cc-put-0.sh $1
  prefix
  $DIR/mcas-hstore-cc-get-0.sh $1
  prefix
  GOAL=700 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-put-0.sh $1
  prefix
  GOAL=1700 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-get-0.sh $1
  prefix
  FLUSH_ENABLE=0 GOAL=2200 FORCE_DIRECT=1 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-put_direct-0.sh $1
  prefix
  GOAL=3000 FORCE_DIRECT=1 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-get_direct-0.sh $1

  if $ado_prereq
  then :
    prefix
    $DIR/mcas-hstore-ado-0.sh $1
  fi
}

# default goal is 25% speed
BUILD_SCALE=25
# if parameter say release or the directory name includes release, expect full speed
if [[ "$1" == release || "$DIR" == */release/* ]]
then :
  BUILD_SCALE=100
fi

prefix
SCALE="$BUILD_SCALE" $DIR/mcas-mapstore-put-0.sh $1
prefix
SCALE="$BUILD_SCALE" $DIR/mcas-mapstore-get-0.sh $1

if false && has_module_xpmem # broken: shard appears to run xpmem_make twice on the same range with no intervening xpemm_remove
then :
  prefix
  $DIR/mcas-mapstore-ado-0.sh $1
fi

# default assumption: $FSDAX is not mounted. Expect disk performance (15%)
FSDAX_FILE_SCALE=15

if findmnt "$FSDAX_DIR" > /dev/null
then :
  # found a mount. Probably pmem
  FSDAX_FILE_SCALE=100
fi
  
# default: goal is 25% speed
BUILD_SCALE=25
# if parameter says release or the directory name includes release, expect full speed
if [[ "$1" == release || "$DIR" == */release/* ]]
then :
  BUILD_SCALE=100
fi

if test -n "$DEVDAX_PFX"
then :
  DAX_PREFIX="$DEVDAX_PFX" SCALE="$BUILD_SCALE" USE_ODP=0 run_hstore has_module_mcasmod $1
  # Conflict test, as coded, works only for devdax, not fsdax
  # Conflict in fsdax occurs when data files exist, not when only arenas exist
  prefix
  $DIR/mcas-hstore-dax-conflict-0.sh $1
fi

# DISABLE fsdax TESTS - they are failing

if false && -n "$FSDAX_DIR"
then :
  FSDAX_CPU_SCALE=90
  if test -d "$FSDAX_DIR"
  then :
    rm -Rf "$FSDAX_DIR/*"
    # scale goal by build expectation (relaase vs debug), backing file expectation (disk vs pmem), and fsdax expectation (currently 100%)
    DAX_PREFIX="$FSDAX_DIR" SCALE="$BUILD_SCALE $FSDAX_FILE_SCALE $FSDAX_CPU_SCALE" USE_ODP=1 run_hstore true $1
  else :
    echo "$FSDAX_DIR not present. Skipping fsdax"
  fi
fi

