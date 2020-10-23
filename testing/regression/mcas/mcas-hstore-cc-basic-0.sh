#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
STORETYPE=hstore-cc
TESTID="$(basename --suffix .sh -- $0)-$DAXTYPE"
VALUE_LENGTH=8
DESC="hstore-8-$VALUE_LENGTH-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# launch MCAS server
DAX_RESET=1 ./dist/bin/mcas --config "$("./dist/testing/hstore-0.py" "$STORETYPE" "$DAXTYPE" "$NODE_IP" 11911)" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
ELEMENT_COUNT=$(scale_by_transport 2000000)
STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*120/10)) # too small
STORE_SIZE=$((ELEMENT_COUNT*(8+VALUE_LENGTH)*128/10)) # sufficient
CLIENT_LOG="test$TESTID-client.log"

./dist/bin/kvstore-perf --cores "$(clamp_cpu 3)" --src_addr $NODE_IP --server $NODE_IP --port 11911 \
                        --test put --component mcas --elements $ELEMENT_COUNT --size $STORE_SIZE --skip_json_reporting \
                        --key_length 8 --value_length $VALUE_LENGTH --debug_level $DEBUG &> $CLIENT_LOG &

CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?

# check result
if [ "$1" == "release" ]; then
    GOAL=190000 # test machine
else
    GOAL=45000
fi

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $DESC $GOAL
