#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

TESTID=$(basename --suffix .sh -- $0)
DESC=hstore-8-8

NODE_IP=$(ip addr | grep -Po 'inet \K10.0.0.[0-9]+' | head -1)

# launch MCAS server
DAX_RESET=1 ./dist/bin/mcas --conf ./dist/testing/hstore-cc-0.conf --debug 0 &> test$TESTID-server.log &
SERVER_PID=$!

sleep 3

#launch client
./dist/bin/kvstore-perf --port 11911 --cores 14 --server $NODE_IP --test put --component mcas --elements 2000000 --size 500000 --skip_json_reporting --device_name mlx5_0 --key_length 8 --value_length 8 --debug_level 0 &> test$TESTID-client.log &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

# check result
iops=$(cat test$TESTID-client.log | grep -Po 'IOPS: \K[0-9]*')
echo "Test $TESTID: $DESC IOPS ($iops)"

if [ "$1" == "release" ]; then
    LIMIT="220000"
else
    LIMIT="95000"
fi
if [ -z "$iops" ]; then
    echo -e "Test $TESTID: \e[31mfail (no data)\e[0m"
elif [ "$iops" -lt $LIMIT ]; then
    echo -e "Test $TESTID: \e[31mfail ($iops of $LIMIT IOPS)\e[0m"
else
    echo -e "Test $TESTID: \e[32mpassed\e[0m"
fi

