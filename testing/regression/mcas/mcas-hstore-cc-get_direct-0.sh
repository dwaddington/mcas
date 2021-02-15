#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
TESTID="$(basename --suffix .sh -- $0)-$DAXTYPE"
STORETYPE=hstore-cc
VALUE_LENGTH=3000000
# kvstore-keylength-valuelength-store-netprovider
DESC="hstore-8-$VALUE_LENGTH-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

CONFIG_STR="$("./dist/testing/hstore-0.py" "$STORETYPE" "$DAXTYPE" "$NODE_IP")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \`"$CONFIG_STR"\` --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
ELEMENT_COUNT=$(scale_by_transport 2000)
STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*28/10)) # too small
STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*32/10)) # sufficient
CLIENT_LOG="test$TESTID-client.log"
[ 0 -lt $DEBUG ] && echo ./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP --test get_direct --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting --key_length 8 --value_length $VALUE_LENGTH --debug_level $DEBUG
./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP --test get_direct --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting --key_length 8 --value_length $VALUE_LENGTH --debug_level $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?
wait $CLIENT_PID
wait $SERVER_PID

# check result
# GOALs for large (3MB) data
if [ "$1" == "release" ]; then
    GOAL=50
else
    GOAL=50
fi

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $DESC $GOAL
