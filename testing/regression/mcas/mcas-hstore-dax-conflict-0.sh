#!/bin/bash

# test server's detection of two servers using the same /dev/dax devices

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
STORETYPE=hstore
TESTID="$(basename --suffix .sh -- $0)-$DAXTYPE"
VALUE_LENGTH=8
# kvstore-keylength-valuelength-store-netprovider
DESC="hstore-8-$VALUE_LENGTH-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

CONFIG_STR_1="$("./dist/testing/hstore-0.py" "$STORETYPE" "$DAXTYPE" "$NODE_IP" 11911)"
CONFIG_STR_2="$("./dist/testing/hstore-0.py" "$STORETYPE" "$DAXTYPE" "$NODE_IP" 11922)"
# launch first MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR_1"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR_1" --forced-exit --debug $DEBUG &> test$TESTID-server1.log &
SERVER_PID=$!
sleep 3
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR_2"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR_2" --forced-exit --debug $DEBUG &> test$TESTID-server2.log &
SERVER2_PID=$!

sleep 3

kill $SERVER_PID
kill $SERVER2_PID

# arm cleanup
trap "kill -9 $SERVER_PID $SERVER2_PID &> /dev/null" EXIT

# wait for client to complete
wait $SERVER_PID
wait $SERVER2_PID

if cat test$TESTID-server2.log | grep -q 'Resource temporarily unavailable' ; then
    echo "Test $TESTID ($DESC): $(color_pass passed)"
else
    echo "Test $TESTID ($DESC): $(color_fail fail)"
fi

