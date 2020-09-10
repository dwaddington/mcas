#!/bin/bash
ulimit -a > ulimit.log
env > env.log
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
DELAY=8
# run each test
$DIR/mcas-mapstore-basic-0.sh $1
sleep $DELAY
$DIR/mcas-hstore-basic-0.sh $1
sleep $DELAY
$DIR/mcas-hstore-devdax-conflict-0.sh $1
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
sleep $DELAY
$DIR/mcas-mapstore-ado-0.sh $1
sleep $DELAY
$DIR/mcas-hstore-ado-0.sh $1

