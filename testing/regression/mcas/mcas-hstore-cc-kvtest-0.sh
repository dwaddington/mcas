#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAX_PREFIX="${DAX_PREFIX:-$(choose_dax)}"
STORETYPE=hstore-cc
TESTID="$(basename --suffix .sh -- $0)-$(dax_type $DAX_PREFIX)"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

NUMA_NODE=$(numa_node $DAX_PREFIX)
CONFIG_STR="$("./dist/testing/cfg_hstore.py" "$NODE_IP" "$STORETYPE" "$DAX_PREFIX" --numa-node "$NUMA_NODE")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
CLIENT_LOG="test$TESTID-client.log"
# OBJECT_COUNT=5531 is experimental limit for PoolCapacity test using hstore-cc allocator
[ 0 -lt $DEBUG ] && echo OBJECT_COUNT=5500 ./dist/bin/kv-test --server $NODE_IP  --src_addr $NODE_IP --debug $DEBUG
OBJECT_COUNT=5500 ./dist/bin/kv-test --server $NODE_IP  --src_addr $NODE_IP --debug $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?

# check result

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_fail $CLIENT_LOG $TESTID
