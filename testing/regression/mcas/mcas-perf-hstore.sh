#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
# testname-keylength-valuelength-store-netprovider
TESTID="mcas-$STORE-$PERFTEST-$KEY_LENGTH-$VALUE_LENGTH-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

CONFIG_STR="$("./dist/testing/hstore-0.py" "$STORE" "$DAXTYPE" "$NODE_IP")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
CLIENT_LOG="test$TESTID-client.log"
ELEMENT_COUNT=$(scale_by_transport ELEMENT_COUNT)

[ 0 -lt $DEBUG ] && echo ./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test $PERFTEST --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting \
                        --key_length $KEY_LENGTH --value_length $VALUE_LENGTH --debug_level $DEBUG
./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test $PERFTEST --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting \
                        --key_length $KEY_LENGTH --value_length $VALUE_LENGTH --debug_level $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
[ 0 -ne $CLIENT_RC ] && kill $SERVER_PID
wait $SERVER_PID; SERVER_RC=$?

# check result

if [ "$1" != "release" ]; then GOAL=$((GOAL/4)); fi
pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $GOAL
