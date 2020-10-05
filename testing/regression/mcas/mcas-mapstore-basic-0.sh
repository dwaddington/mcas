#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"


TESTID=$(basename --suffix .sh -- $0)
DESC=$TESTID
VALUE_LENGTH=8

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# launch MCAS server
./dist/bin/mcas --config "$("./dist/testing/mapstore-0.py" "$NODE_IP")" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
ELEMENT_COUNT=$(scale_by_transport 2000000)
#STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*84/10)) # too small - mapstore?
STORE_SIZE=$((ELEMENT_COUNT*2000)) # shouldn't need this - efficiency issue?
CLIENT_LOG="test$TESTID-client.log"
./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP --test put --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting --key_length 8 --value_length $VALUE_LENGTH --debug_level $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?

# check result

if [ "$1" == "release" ]; then
    GOAL=165000 # was 220K
else
    GOAL=65000
fi

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $DESC $GOAL
