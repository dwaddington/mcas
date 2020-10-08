#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
STORETYPE=hstore
TESTID="$(basename --suffix .sh -- $0)-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# launch MCAS server
DAX_RESET=1 ./dist/bin/mcas --config "$("./dist/testing/hstore-ado0.py" "$STORETYPE" "$DAXTYPE" "$NODE_IP")" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

# give time to start server
sleep 3

CLIENT_LOG="test$TESTID-client.log"

# launch client
./dist/bin/ado-test --src_addr "$NODE_IP" --server $NODE_IP --debug $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for server to complete
wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?

pass_fail_by_code client $CLIENT_RC server $CLIENT_RC && pass_fail $CLIENT_LOG $TESTID $TESTID
