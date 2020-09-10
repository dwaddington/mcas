#!/bin/bash

# test server's detection of two servers using the same /dev/dax devices

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

TESTID=$(basename --suffix .sh -- $0)
DAXTYPE="$(choose_dax_type)"
VALUE_LENGTH=8
# kvstore-keylength-valuelength-store-netprovider
DESC="hstore-8-$VALUE_LENGTH-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# parameters for MCAS server
SERVER_CONFIG="hstore-$DAXTYPE-0"

# launch first MCAS server
DAX_RESET=1 ./dist/bin/mcas --config "$("./dist/testing/$SERVER_CONFIG.py" "$NODE_IP" 11911)" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!
sleep 3
DAX_RESET=1 ./dist/bin/mcas --config "$("./dist/testing/$SERVER_CONFIG.py" "$NODE_IP" 11922)" --forced-exit --debug 2 &> test$TESTID-server2.log &
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

