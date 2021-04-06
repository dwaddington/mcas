#!/bin/bash
ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"
DELAY=4

run_minimal_hstore() {
  typeset ado_prereq="$1"
  shift
 # run each test
  sleep $DELAY
  $DIR/mcas-hstore-put-0.sh $1
  sleep $DELAY
  $DIR/mcas-hstore-cc-put-0.sh $1
  if $ado_prereq
  then sleep $DELAY
    $DIR/mcas-hstore-ado-0.sh $1
  fi
}

$DIR/mcas-mapstore-put-0.sh $1
if has_module_xpmem
then sleep $DELAY
  $DIR/mcas-mapstore-ado-0.sh $1
fi
sleep $DELAY

if has_devdax
then DAXTYPE=devdax run_minimal_hstore has_module_mcasmod $1
  sleep $DELAY
  # Conflict test, as coded, works only for devdax, not fsdax
  # Conflict in fsdax occurs when data files exist, not when only arenas exist
  $DIR/mcas-hstore-dax-conflict-0.sh $1
fi

if has_fsdax
then DAXTYPE=fsdax USE_ODP=1 run_minimal_hstore true $1
fi

