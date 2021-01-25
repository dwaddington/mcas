#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

DAXTYPE="${DAXTYPE:-$(choose_dax_type)}"
STORETYPE=hstore
VALUE_LENGTH=${VALUE_LENGTH:-4096}
KEY_LENGTH=${KEY_LENGTH:-8}
READ_PCT=${READ_PCT:-0}
TESTID="$(basename --suffix .sh -- $0)-$DAXTYPE-L$VALUE_LENGTH-R${READ_PCT}"
# kvstore-keylength-valuelength-store-netprovider
DESC="hstore-8-$VALUE_LENGTH-$DAXTYPE"

# parameters for MCAS server and client
NODE_IP="$(node_ip)"
DEBUG=${DEBUG:-0}

# launch MCAS server
DAX_RESET=1 ./dist/bin/mcas --config "$("./dist/testing/hstore-0.py" "$STORETYPE" "$DAXTYPE" "$NODE_IP")" --forced-exit --debug $DEBUG &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

# launch client
RAW_STORE_SIZE=${RAW_STORE_SIZE:-25231179776} # size of /mnt/pmem1 on ribbit1
RAW_STORE_SIZE=${RAW_STORE_SIZE:-25365053440} # size of /dev/dax0.0 on ribbit1
USABLE_STORE_SIZE=$((RAW_STORE_SIZE*9/10))
EXPANSION=20 # usually sufficient, but not for 4K --read_pct 0 or 50 location 48791
EXPANSION=24 # usually sufficient, but not for 4K --read_pct 0 location 9873
EXPANSION=26 # sufficient

ELEMENT_COUNT=$((USABLE_STORE_SIZE / ( (KEY_LENGTH+VALUE_LENGTH)*EXPANSION/10 ) ))
if [ 100000 -lt $ELEMENT_COUNT ]
then ELEMENT_COUNT=100000
fi

# adjust elements so that USABLE_STORE_SIZE * elements is not more than available storage
CLIENT_LOG="test$TESTID-client.log"
./dist/bin/kvstore-perf --cores "$(clamp_cpu 14)" --src_addr $NODE_IP --server $NODE_IP --test throughput --read_pct ${READ_PCT} --component mcas --size $USABLE_STORE_SIZE --skip_json_reporting --key_length $KEY_LENGTH --value_length $VALUE_LENGTH --elements=$ELEMENT_COUNT --duration 30 --debug_level $DEBUG &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete

wait $CLIENT_PID; CLIENT_RC=$?
wait $SERVER_PID; SERVER_RC=$?

# check result
# GOALs for large (3MB) data
if [ "$1" == "release" ]; then
    GOAL=10
else
    GOAL=10
fi

pass_fail_by_code client $CLIENT_RC server $SERVER_RC && pass_by_iops $CLIENT_LOG $TESTID $DESC $GOAL
