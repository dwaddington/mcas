#!/bin/bash
NODE_IP=$1
port=$2
#launch client
cd /tmp/mcas/build

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

CLIENT_LOG=/tmp/mcas/testhstore-basic-client.log 

./dist/bin/kvstore-perf --port $port --cores 14 --server $NODE_IP --test put --component mcas --elements 2000000 --size 400000000 --skip_json_reporting --device_name mlx5_0 --key_length 8 --value_length 8 --debug_level 0 &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

# check result
iops=$(cat $CLIENT_LOG | grep -Po 'IOPS: \K[0-9]*')
echo "Test hstore basic: IOPS ($iops)" | tee -a /tmp/mcas/results.log

if [ "$1" == "release" ]; then
    LIMIT="220000"
else
    LIMIT="92000"
fi
if [ -z "$iops" ]; then
    echo -e "Test hstore basic: \e[31mfail (no data)\e[0m" | tee -a /tmp/mcas/results.log
elif [ "$iops" -lt $LIMIT ]; then
    echo -e "Test hstore basic: \e[31mfail ($iops of $LIMIT IOPS)\e[0m" | tee -a /tmp/mcas/results.log
else
    echo -e "Test hstore basic: \e[32mpassed\e[0m" | tee -a /tmp/mcas/results.log
fi
