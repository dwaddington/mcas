#!/bin/bash -eu

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

declare -A expansion
expansion[hstore]=100
expansion[hstore-cc]=240
expansion_factor=${expansion[$STORE]}

KEY_LENGTH=${KEY_LENGTH:-8}
STORE_AVAILABLE=${STORE_AVAILABLE:-100000000}
MAX_ELEMENT_COUNT=$((STORE_AVAILABLE*100/expansion_factor/(200+KEY_LENGTH+VALUE_LENGTH)))
if [ $MAX_ELEMENT_COUNT -lt $ELEMENT_COUNT ]
then ELEMENT_COUNT=$MAX_ELEMENT_COUNT
fi

STORE_SIZE=${STORE_SIZE:-$STORE_AVAILABLE} \
SCALE=${SCALE:-100} \

DAX_PREFIX="${DAX_PREFIX:-$(choose_dax)}"
# testname-keylength-valuelength-store-netprovider
TESTID="mcas-$STORE-$PERFTEST-$KEY_LENGTH-$VALUE_LENGTH-$(dax_type $DAX_PREFIX)"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}
PERF_OPTS=${PERF_OPTS:-"--skip_json_reporting"}

CONFIG_STR="$("./dist/testing/cfg_hstore.py" "$NODE_IP" "$STORE" "$DAX_PREFIX" --numa-node "$(numa_node $DAX_PREFIX)")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
CLIENT_LOG="test$TESTID-client.log"
ELEMENT_COUNT=$(scale_by_transport $ELEMENT_COUNT)
ELEMENT_COUNT=$(scale $ELEMENT_COUNT $SCALE)

[ 0 -lt $DEBUG ] && echo ./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test $PERFTEST --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE ${PERF_OPTS} \
                        --key_length $KEY_LENGTH --value_length $VALUE_LENGTH --debug_level $DEBUG
./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP \
                        --test $PERFTEST --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE ${PERF_OPTS} \
                        --key_length $KEY_LENGTH --value_length $VALUE_LENGTH --debug_level $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "set +e; kill -s KILL $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
[ 0 -ne $CLIENT_RC ] && kill $SERVER_PID
wait $SERVER_PID; SERVER_RC=$?

# check result

GOAL=$(scale $GOAL $SCALE)
pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $GOAL
