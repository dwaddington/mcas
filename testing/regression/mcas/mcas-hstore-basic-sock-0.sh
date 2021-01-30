#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="$(choose_dax_type)"
TESTID="$(basename --suffix .sh -- $0)-$DAXTYPE"
VALUE_LENGTH=8
# kvstore-keylength-valuelength-store-netprovider
DESC=hstore-8-8-$DAXTYPE-sock

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# parameters for MCAS server
SERVER_CONFIG="hstore-$DAXTYPE-sock-0"

CONFIG_STR="$("./dist/testing/$SERVER_CONFIG.py" "$NODE_IP")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \`"$CONFIG_STR"\` --forced-exit --debug $DEBUG &> test$TESTID-server.log &
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

SOCKET_SCALE=1000

# launch client
ELEMENT_COUNT=$(scale_by_transport 2000000 $SOCKET_SCALE)
STORE_SIZE=$((ELEMENT_COUNT*VALUE_LENGTH*24/10))
CLIENT_LOG="test$TESTID-client.log"
[ 0 -lt $DEBUG ] && echo ./dist/bin/kvstore-perf --provider sockets --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test put --component mcas --elements $ELEMENT_COUNT \
                        --size $STORE_SIZE --skip_json_reporting --key_length 8 --value_length $VALUE_LENGTH \
                        --debug_level $DEBUG &> $CLIENT_LOG &
./dist/bin/kvstore-perf --provider sockets --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test put --component mcas --elements $ELEMENT_COUNT \
                        --size $STORE_SIZE --skip_json_reporting --key_length 8 --value_length $VALUE_LENGTH \
                        --debug_level $DEBUG &> $CLIENT_LOG &
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
    GOAL=85000
fi

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $DESC $GOAL $SOCKET_SCALE
