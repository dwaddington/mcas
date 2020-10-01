#!/bin/bash
ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
. "$DIR/functions.sh"
DELAY=8

run_hstore() {
 # run each test
  sleep $DELAY
  $DIR/mcas-hstore-basic-0.sh $1
  sleep $DELAY
  $DIR/mcas-hstore-get_direct-0.sh $1
  sleep $DELAY
  $DIR/mcas-hstore-put_direct-0.sh $1
  sleep $DELAY
  # includes async_put, async_erase, async_put_direct
  $DIR/mcas-hstore-cc-kvtest-0.sh $1
  sleep $DELAY
  $DIR/mcas-hstore-cc-basic-0.sh $1
  sleep $DELAY
  $DIR/mcas-hstore-cc-get_direct-0.sh $1
  sleep $DELAY
  $DIR/mcas-hstore-cc-put_direct-0.sh $1
}

$DIR/mcas-mapstore-basic-0.sh $1
sleep $DELAY
$DIR/mcas-mapstore-ado-0.sh $1
sleep $DELAY

if has_devdax
then DAXTYPE=devdax run_hstore $1
  sleep $DELAY
  # Conflici, as coded works only for devdax, not fsdax
  # Conflict in fsdax occurs when data files exist, not when only arenas exist
  $DIR/mcas-hstore-dax-conflict-0.sh $1
  sleep $DELAY
  # ADO does not net work with fsdax
  $DIR/mcas-hstore-ado-0.sh $1
fi

if has_fsdax
then DAXTYPE=fsdax run_hstore $1
fi

