#!/bin/bash
ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"
DELAY=8

FSDAX_DIR=/mnt/pmem1

prefix()
{
  : ls -lr $FSDAX_DIR
  sleep $DELAY
}

run_hstore() {
  typeset ado_prereq="$1"
  shift
 # run each test
  prefix
  GOAL=195000 ELEMENT_COUNT=2000000 STORE=hstore PERFTEST=put $DIR/mcas-hstore-put-0.sh $1
  prefix
  GOAL=200000 ELEMENT_COUNT=2000000 STORE=hstore PERFTEST=get $DIR/mcas-hstore-get-0.sh $1
  prefix
  GOAL=750 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-put-0.sh $1
  prefix
  GOAL=2000 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-get-0.sh $1
  prefix
  GOAL=1250 FORCE_DIRECT=1 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-put_direct-0.sh $1
  prefix
  GOAL=1900 FORCE_DIRECT=1 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-get_direct-0.sh $1
  prefix
  # includes async_put, async_erase, async_put_direct
  $DIR/mcas-hstore-cc-kvtest-0.sh $1
  prefix
  $DIR/mcas-hstore-cc-put-0.sh $1
  prefix
  $DIR/mcas-hstore-cc-get-0.sh $1
  prefix
  GOAL=750 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-put-0.sh $1
  prefix
  GOAL=2000 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-get-0.sh $1
  prefix
  GOAL=1900 FORCE_DIRECT=1 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-put_direct-0.sh $1
  prefix
  GOAL=1250 FORCE_DIRECT=1 ELEMENT_COUNT=10000 VALUE_LENGTH=2000000 $DIR/mcas-hstore-cc-get_direct-0.sh $1
  prefix
  if $ado_prereq
  then :
    prefix
    $DIR/mcas-hstore-ado-0.sh $1
  fi
}

prefix
$DIR/mcas-mapstore-put-0.sh $1
prefix
$DIR/mcas-mapstore-get-0.sh $1
if has_module_xpmem
then :
  prefix
  $DIR/mcas-mapstore-ado-0.sh $1
fi

if has_devdax
then :
  DAXTYPE=devdax USE_ODP=0 run_hstore has_module_mcasmod $1
  # Conflict test, as coded, works only for devdax, not fsdax
  # Conflict in fsdax occurs when data files exist, not when only arenas exist
  prefix
  $DIR/mcas-hstore-dax-conflict-0.sh $1
fi

if has_fsdax
then :
  if test -d "$FSDAX_DIR"
  then :
    if df "$FSDAX_DIR" > /dev/null
    then :
      DAXTYPE=fsdax USE_ODP=1 run_hstore true $1
    else :
      echo "iNo filesystem mounted on $FSDAX_DIR. Skipping fsdax"
    fi
  else :
    echo "$FSDAX_DIR not present. Skipping fsdax"
  fi
fi

