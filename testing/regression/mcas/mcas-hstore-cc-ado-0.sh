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
CONFIG_STR="$("./dist/testing/cfg_hstore_ado.py" "$NODE_IP" "$STORETYPE" "$DAX_PREFIX" --numa-node "$NUMA_NODE")"
# launch MCAS server
[ 0 -lt $DEBUG ] && echo DAX_RESET=1 ./dist/bin/mcas --config \'"$CONFIG_STR"\' --forced-exit --debug $DEBUG
DAX_RESET=1 ./dist/bin/mcas --config "$CONFIG_STR" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

# give time to start server
sleep 3

CLIENT_LOG="test$TESTID-client.log"

# launch client
[ 0 -lt $DEBUG ] && echo ./dist/bin/ado-test --src_addr "$NODE_IP" --server $NODE_IP --port 11911 --debug $DEBUG
./dist/bin/ado-test --src_addr "$NODE_IP" --server $NODE_IP --port 11911 --debug $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_fail $CLIENT_LOG $TESTID
