#!/bin/bash

prefix()
{
  : ls -lr $FSDAX_DIR
  sleep $DELAY
}

run_hstore() {
  typeset ado_prereq="$1"
  shift
  if [[ 0 -lt $scope ]]
  then :
    # mapstore unit tests: basic
    [ 0 -lt "$DEBUG" ] && echo ./src/components/store/test/store-test1 --store mapstore --many-count 100000
                               ./src/components/store/test/store-test1 --store mapstore --many-count 100000
    # hstore unit tests: basic
    STORE_LOCATION="$("$DIR/dax.py" --prefix "$DAX_PREFIX")"
    [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore --many-count 100000 --daxconfig \'"$STORE_LOCATION"\'
                               DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore --many-count 100000 --daxconfig "$STORE_LOCATION"
    [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-cc --daxconfig \'"$STORE_LOCATION"\' --many-count 100000
                               DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-cc --daxconfig "$STORE_LOCATION" --many-count 100000
    [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-mm --mm-plugin-path ./dist/lib/libmm-plugin-ccpm.so --daxconfig \'"$STORE_LOCATION"\' --many-count 100000
                               DAX_RESET=1 ./src/components/store/test/store-test1 --store hstore-mm --mm-plugin-path ./dist/lib/libmm-plugin-ccpm.so --daxconfig "$STORE_LOCATION" --many-count 100000

    # hstore unit tests: multithreaded lock/unlock
    [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 STORE=hstore-mt STORE_LOCATION=\'"$STORE_LOCATION"\' MM_PLUGIN_PATH=./dist/lib/libmm-plugin-ccpm.so ./src/components/store/hstore/unit_test/hstore-testmt
                               DAX_RESET=1 STORE=hstore-mt STORE_LOCATION="$STORE_LOCATION" MM_PLUGIN_PATH=./dist/lib/libmm-plugin-ccpm.so ./src/components/store/hstore/unit_test/hstore-testmt
    [ 0 -lt "$DEBUG" ] && echo DAX_RESET=1 STORE=hstore-mt STORE_LOCATION=\'"$STORE_LOCATION"\' MM_PLUGIN_PATH=./dist/lib/libmm-plugin-rcalb.so ./src/components/store/hstore/unit_test/hstore-testmt
                               DAX_RESET=1 STORE=hstore-mt STORE_LOCATION="$STORE_LOCATION" MM_PLUGIN_PATH=./dist/lib/libmm-plugin-rcalb.so ./src/components/store/hstore/unit_test/hstore-testmt
    # run performance tests
  fi
  prefix
  GOAL=140000 ELEMENT_COUNT=2000000 STORE=hstore PERFTEST=put $DIR/mcas-hstore-put-0.sh ${1+"$@"}
  if [[ 0 -lt $scope ]]
  then :
    prefix
    GOAL=170000 ELEMENT_COUNT=2000000 STORE=hstore PERFTEST=get $DIR/mcas-hstore-get-0.sh ${1+"$@"}
    prefix
    GOAL=900 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-put-0.sh ${1+"$@"}
    prefix
    GOAL=1750 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-get-0.sh ${1+"$@"}
    prefix
    FLUSH_ENABLE=0 GOAL=2400 FORCE_DIRECT=1 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-put_direct-0.sh ${1+"$@"}
    prefix
    GOAL=3000 FORCE_DIRECT=1 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-get_direct-0.sh ${1+"$@"}
    prefix
    # includes async_put, async_erase, async_put_direct
    $DIR/mcas-hstore-cc-kvtest-0.sh ${1+"$@"}
    prefix
    $DIR/mcas-hstore-mc-kvtest-0.sh ${1+"$@"}
    prefix
    $DIR/mcas-hstore-mr-kvtest-0.sh ${1+"$@"}
    prefix
    $DIR/mcas-hstore-cc-put-0.sh ${1+"$@"}
    prefix
    $DIR/mcas-hstore-cc-get-0.sh ${1+"$@"}
  fi
  prefix
  GOAL=700 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-put-0.sh ${1+"$@"}
  if [[ 0 -lt $scope ]]
  then :
    prefix
    GOAL=1700 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-get-0.sh ${1+"$@"}
    prefix
    FLUSH_ENABLE=0 GOAL=2200 FORCE_DIRECT=1 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-put_direct-0.sh ${1+"$@"}
    prefix
    GOAL=3000 FORCE_DIRECT=1 ELEMENT_COUNT=2000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-get_direct-0.sh ${1+"$@"}
  fi
  if $ado_prereq
  then :
    prefix
    $DIR/mcas-hstore-ado-0.sh ${1+"$@"}
  else
    echo "mcasmod module not loaded. Skipping devdax ADO"
  fi
}

ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"

set -eu

DEBUG=${DEBUG:-0}
# 0: don't test
# 1: test unless disabled by -f
# 2: definitely test
typeset -i test_fsdax=0
typeset -i test_devdax=1
# 0: run a minimal set of tests
# 1: run standard set of tests
typeset -i scope=1
typeset build=
declare -a positionals=()
while [[ $# -ne 0 ]]
do
  case $1 in
    -f|--fsdax)
      test_fsdax=2
      if [[ $test_devdax -lt 2 ]]
      then test_devdax=0
      fi
      shift
      ;;
    -m|--minimal)
      scope=0
      shift
      ;;
    -d|--devdax)
      test_devdax=2
      shift
      ;;
    -b|--build)
      shift
      build="$1"
      shift
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    release|debug)
      build="$1"
      shift
      ;;
    *)
      positionals+=("$1")
      shift
      ;;
  esac
done

set -- "${positionals[@]}"

DELAY=8

FSDAX_DIR=$(find_fsdax)
DEVDAX_PFX=$(find_devdax)

# default goal is 25% speed
BUILD_SCALE=25
# if parameter say release or the directory name includes release, expect full speed
if [[ "$build" == release || "$DIR" == */release/* ]]
then :
  BUILD_SCALE=100
fi

prefix
SCALE="$BUILD_SCALE" $DIR/mcas-mapstore-put-0.sh $build
prefix
SCALE="$BUILD_SCALE" $DIR/mcas-mapstore-get-0.sh $build

if false && has_module_xpmem # broken: shard appears to run xpmem_make twice on the same range with no intervening xpemm_remove
then :
  prefix
  $DIR/mcas-mapstore-ado-0.sh $build
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
if [[ "$build" == release || "$DIR" == */release/* ]]
then :
  BUILD_SCALE=100
fi

if [[ $test_devdax -eq 0 ]]
then :
  echo "dexdax test disabled Skipping devdax"
elif [[ -z "$DEVDAX_PFX" ]]
then :
  echo "${DEVDAX_PFX}* device not present. Skipping devdax"
else :
  DAX_PREFIX="$DEVDAX_PFX" SCALE="$BUILD_SCALE" USE_ODP=0 run_hstore has_module_mcasmod $build
  # Conflict test, as coded, works only for devdax, not fsdax
  # Conflict in fsdax occurs when data files exist, not when only arenas exist
  if [[ 0 -lt $scope ]]
  then :
    prefix
    $DIR/mcas-hstore-dax-conflict-0.sh $build
  fi
fi

# DISABLE fsdax TESTS - they are failing

if [[ $test_fsdax -eq 0 ]]
then :
  echo "fsdax test disabled. Skipping fsdax"
elif [[ -z "$FSDAX_DIR" ]]
then :
  echo "$FSDAX_DIR not present. Skipping fsdax"
else :
  FSDAX_CPU_SCALE=90
  if test -d "$FSDAX_DIR"
  then :
    rm -rf "$FSDAX_DIR"/a0
    mkdir "$FSDAX_DIR"/a0 # the directory expected by the dax.py object for fsdax
    # scale goal by build expectation (relaase vs debug), backing file expectation (disk vs pmem), and fsdax expectation (currently 100%)
    DAX_PREFIX="$FSDAX_DIR" SCALE="$BUILD_SCALE $FSDAX_FILE_SCALE $FSDAX_CPU_SCALE" USE_ODP=1 run_hstore true $build
  fi
fi
