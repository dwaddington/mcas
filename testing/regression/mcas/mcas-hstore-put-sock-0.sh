#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
STORETYPE=hstore
KEY_LENGTH=${KEY_LENGTH:-8}
VALUE_LENGTH=${VALUE_LENGTH:-8}
SOCKET_SCALE=1000
RECOMMENDED_ELEMENT_COUNT=$(scale_by_transport 2000000 $SOCKET_SCALE)
PERFTEST=put
RECOMMENDED_GOAL=185000 # wildly high for socket test
# testname-keylength-valuelength-store-netprovider
TESTID="$(basename --suffix .sh -- $0)-$KEY_LENGTH-$VALUE_LENGTH-$DAXTYPE-sock"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# parameters for MCAS server
SERVER_CONFIG="hstore-$DAXTYPE-sock-0"

CONFIG_STR="$("./dist/testing/$SERVER_CONFIG.py" "$NODE_IP")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
ELEMENT_COUNT=${ELEMENT_COUNT:-$RECOMMENDED_ELEMENT_COUNT}
RECOMMENDED_STORE_SIZE=$((ELEMENT_COUNT*(200+KEY_LENGTH+VALUE_LENGTH)*10/10)) # sufficient
STORE_SIZE=${STORE_SIZE:-$RECOMMENDED_STORE_SIZE}
CLIENT_LOG="test$TESTID-client.log"

[ 0 -lt $DEBUG ] && echo ./dist/bin/kvstore-perf --provider sockets --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test $PERFTEST --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting \
                        --key_length $KEY_LENGTH --value_length $VALUE_LENGTH --debug_level $DEBUG
./dist/bin/kvstore-perf --provider sockets --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
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

if [ "$1" != "release" ]; then RECOMMENDED_GOAL=$((RECOMMENDED_GOAL/4)); fi
GOAL=${GOAL:-RECOMMENDED_GOAL}
pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $GOAL $SOCKET_SCALE
