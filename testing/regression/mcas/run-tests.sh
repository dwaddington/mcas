#!/bin/bash -eu

prefix()
{
  : ls -lr $FSDAX_DIR
  sleep $DELAY
}

run_hstore() {
  echo STORE_AVAILABLE $STORE_AVAILABLE
  typeset ado_prereq="$1"
  shift
  if [[ 0 -ne $test_hstore ]]
  then :
    if [[ 0 -ne ${run_suite[unit]} ]]
    then :
      echo "RUN hstore unit tests"
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
    fi
    if [[ 0 -ne ${run_suite[kvfunction]} ]]
    then :
      echo "RUN hstore kvtest function tests"
      # includes async_put, async_erase, async_put_direct
      for st in hstore hstore-cc hstore-mc hstore-mr 
      do :
        prefix
        $DIR/mcas-$st-kvtest-0.sh ${1+"$@"}
      done
    fi
    if [[ 0 -ne ${run_suite[performance]} ]]
    then :
      # run performance tests
      echo "RUN hstore kvtest performance tests"
      # hstore (reconsituting allocator)
      # small values
      prefix
      GOAL=140000 ELEMENT_COUNT=2000000 VALUE_LENGTH=8 STORE=hstore PERFTEST=put $DIR/mcas-perf-hstore.sh ${1+"$@"}
      prefix
      GOAL=170000 ELEMENT_COUNT=2000000 VALUE_LENGTH=8 STORE=hstore PERFTEST=get $DIR/mcas-perf-hstore.sh ${1+"$@"}
      # large values
      prefix
      GOAL=900 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore PERFTEST=put $DIR/mcas-perf-hstore.sh ${1+"$@"}
      prefix
      GOAL=1750 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore PERFTEST=get $DIR/mcas-perf-hstore.sh ${1+"$@"}
      # large values, direct
      prefix
      GOAL=2400 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore PERFTEST=put_direct FORCE_DIRECT=1 FLUSH_ENABLE=0 $DIR/mcas-perf-hstore.sh ${1+"$@"}
      prefix
      GOAL=3000 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore PERFTEST=get_direct FORCE_DIRECT=1 $DIR/mcas-perf-hstore.sh ${1+"$@"}
      # hstore (crash-consistent allocator)
      # small values
      prefix
      GOAL=130000 ELEMENT_COUNT=2000000 VALUE_LENGTH=8 STORE=hstore-cc PERFTEST=put $DIR/mcas-perf-hstore.sh ${1+"$@"}
      prefix
      GOAL=130000 ELEMENT_COUNT=2000000 VALUE_LENGTH=8 STORE=hstore-cc PERFTEST=get $DIR/mcas-perf-hstore.sh ${1+"$@"}
      # large values
      prefix
      GOAL=700 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore-cc PERFTEST=put $DIR/mcas-perf-hstore.sh ${1+"$@"}
      prefix
      GOAL=1700 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore-cc PERFTEST=get $DIR/mcas-perf-hstore.sh ${1+"$@"}
      # large values, direct
      prefix
      GOAL=2200 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore-cc PERFTEST=put_direct FORCE_DIRECT=1 FLUSH_ENABLE=0 $DIR/mcas-perf-hstore.sh ${1+"$@"}
      prefix
      GOAL=3000 ELEMENT_COUNT=6000 VALUE_LENGTH=2000000 STORE=hstore-cc PERFTEST=get_direct FORCE_DIRECT=1 $DIR/mcas-perf-hstore.sh ${1+"$@"}
    fi

    if [[ 0 -ne ${run_suite[ado]} ]]
    then :
      if $ado_prereq
      then :
        echo "RUN hstore ADO test"
        prefix
        $DIR/mcas-hstore-ado-0.sh ${1+"$@"}
      else
        echo "SKIP ADO test: kernel module mcasmod module not loaded."
      fi
    fi
  fi
}

ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"

DEBUG=${DEBUG:-0}
# 0: don't test
# 1: test because default
# 2: test because explicitly requested
typeset -i test_fsdax=0
typeset -i test_devdax=1
typeset -i test_hstore=1
typeset -i test_mapstore=1
declare -i -A run_suite
for i in ado kvfunction performance unit; do run_suite[$i]=1; done
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
      if [[ $test_mapstore -lt 2 ]]
      then test_mapstore=0
      fi
      shift
      ;;
    -m|--minimal)
      scope=0
      shift
      ;;
    --mapstore)
      test_mapstore=2
      if [[ $test_hstore -lt 2 ]]
      then test_hstore=0
      fi
      shift
      ;;
    --hstore)
      test_hstore=2
      if [[ $test_mapstore -lt 2 ]]
      then test_mapstore=0
      fi
      shift
      ;;
    -d|--devdax)
      test_devdax=2
      if [[ $test_mapstore -lt 2 ]]
      then test_mapstore=0
      fi
      shift
      ;;
    -a|--ado)
      for i in ado kvfunction performance unit; do if [[ ${run_suite[$i]} -lt 2 ]]; then run_suite[$i]=0; fi; done
      run_suite[ado]=2
      shift
      ;;
    -k|--kvfunction)
      for i in ado kvfunction performance unit; do if [[ ${run_suite[$i]} -lt 2 ]]; then run_suite[$i]=0; fi; done
      run_suite[kvfunction]=2
      shift
      ;;
    -p|--performance)
      for i in ado kvfunction performance unit; do if [[ ${run_suite[$i]} -lt 2 ]]; then run_suite[$i]=0; fi; done
      run_suite[performance]=2
      shift
      ;;
    -u|--unit)
      for i in ado kvfunction performance unit; do if [[ ${run_suite[$i]} -lt 2 ]]; then run_suite[$i]=0; fi; done
      run_suite[unit]=2
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
    -h|--help)
      echo "$0 [-{m|d|f|p|u}] [--minimal] [--devdax] [--fsdax] [--performance] [--unit] [--ado] [--build <type>]"
      exit 0
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

if [[ 0 -ne $test_mapstore ]]
then :
  if [[ 0 -ne ${run_suite[unit]} ]]
  then :
    echo "RUN maptore unit tests"
    # mapstore unit tests: basic
    [ 0 -lt "$DEBUG" ] && echo ./src/components/store/test/store-test1 --store mapstore --many-count 100000
                               ./src/components/store/test/store-test1 --store mapstore --many-count 100000
  else
    echo "SKIP maptore unit tests"
  fi
  if [[ 0 -ne ${run_suite[performance]} ]]
  then :
    echo "RUN maptore performance tests"
    # mapstore performance tests
    [ 0 -lt "$DEBUG" ] && echo SCALE="$BUILD_SCALE" GOAL=150000 PERFTEST=put VALUE_LENGTH=8 ELEMENT_COUNT=2000000 $DIR/mcas-perf-mapstore.sh $build
                               SCALE="$BUILD_SCALE" GOAL=150000 PERFTEST=put VALUE_LENGTH=8 ELEMENT_COUNT=2000000 $DIR/mcas-perf-mapstore.sh $build
    [ 0 -lt "$DEBUG" ] && echo SCALE="$BUILD_SCALE" GOAL=180000 PERFTEST=get VALUE_LENGTH=8 ELEMENT_COUNT=2000000 $DIR/mcas-perf-mapstore.sh $build
                               SCALE="$BUILD_SCALE" GOAL=180000 PERFTEST=get VALUE_LENGTH=8 ELEMENT_COUNT=2000000 $DIR/mcas-perf-mapstore.sh $build
  else
    echo "SKIP maptore performance tests"
  fi
else
  echo "SKIP maptore"
fi

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
  echo "SKIP devdax: test disabled"
elif [[ -z "$DEVDAX_PFX" ]]
then :
  echo "SKIP devdax: ${DEVDAX_PFX}* not present"
else :
  devdax_device_list=$($DIR/devdax.py --prefix ${DEVDAX_PFX})
  if [[ -z "$devdax_device_list" ]]
  then :
     echo "SKIP devdax: fail: ${DEVDAX_PFX}* no eligible devices"
  else
    declare -a devices=($(ls -l $devdax_device_list | awk '{gsub(",",":") ; print $5 $6}'))
    STORE_AVAILABLE=$(cat /sys/dev/char/${devices[0]}/size)

    # DM_region incurs overhead. Reduce available space
    STORE_AVAILABLE=$((STORE_AVAILABLE*95/100))
    STORE_AVAILABLE=$STORE_AVAILABLE DAX_PREFIX="$DEVDAX_PFX" SCALE="$BUILD_SCALE" USE_ODP=0 run_hstore has_module_mcasmod $build
    # Conflict test, as coded, works only for devdax, not fsdax
    # Conflict in fsdax occurs when data files exist, not when only arenas exist
    prefix
    $DIR/mcas-hstore-dax-conflict-0.sh $build
  fi
fi

# DISABLE fsdax TESTS - they are failing

if [[ $test_fsdax -eq 0 ]]
then :
  echo "SKIP fsdax: test disabled"
elif [[ -z "$FSDAX_DIR" ]]
then :
  echo "SKIP fsdax $FSDAX_DIR not present"
else :
  FSDAX_CPU_SCALE=90
  if test -d "$FSDAX_DIR"
  then :
    rm -rf "$FSDAX_DIR"/a0
    mkdir "$FSDAX_DIR"/a0 # the directory expected by the dax.py object for fsdax
    # scale goal by build expectation (relaase vs debug), backing file expectation (disk vs pmem), and fsdax expectation (currently 100%)
    STORE_AVAILABLE=$(($(df -k --output=iavail /mnt/pmem0/ | tail -1)*1024)) DAX_PREFIX="$FSDAX_DIR" SCALE="$BUILD_SCALE $FSDAX_FILE_SCALE $FSDAX_CPU_SCALE" USE_ODP=1 run_hstore true $build
  fi
fi
