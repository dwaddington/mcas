#!/bin/bash
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

# launch MCAS server
DAX_RESET=1 ./dist/bin/mcas --config "$("./dist/testing/$SERVER_CONFIG.py" "$NODE_IP")" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
ELEMENT_COUNT=$(scale_by_transport 2000000)
STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*80/10)) # too small
STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*84/10)) # sufficient
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
    GOAL=185000 # was 220K
else
    GOAL=72000
fi

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $DESC $GOAL
