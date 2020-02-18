#!/bin/bash
NODE_IP=$1
port=$2
cd /tmp/mcas/build
#launch client
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

CLIENT_LOG=/tmp/mcas/testmapstore-basic-client.log 

./dist/bin/kvstore-perf --port $port --cores 14 --server $NODE_IP --test put --component mcas --elements 2000000 --size 500000 --skip_json_reporting --device_name mlx5_0 --key_length 8 --value_length 8 --debug_level 0 &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

# check result
iops=$(cat $CLIENT_LOG | grep -Po 'IOPS: \K[0-9]*')
echo "Test mapstore basic: IOPS ($iops)" | tee -a /tmp/mcas/results.log

if [ "$2" == "release" ]; then
    LIMIT="220000"
else
    LIMIT="90000"
fi
if [ -z "$iops" ]; then
    echo -e "Test mapstore basic: \e[31mfail (no data)\e[0m" | tee -a /tmp/mcas/results.log
elif [ "$iops" -lt $LIMIT ]; then
    echo -e "Test mapstore basic: \e[31mfail ($iops of $LIMIT IOPS)\e[0m" | tee -a /tmp/mcas/results.log
else
    echo -e "Test mapstore basic: \e[32mpassed\e[0m"| tee -a /tmp/mcas/results.log
fi


sleep 5
#ado test

CLIENT_LOG=/tmp/mcas/testmapstore-ado-client.log
#launch client
./dist/bin/ado-test --port $port --server $NODE_IP &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

if cat $CLIENT_LOG | grep -q 'FAILED TEST' ; then
    echo -e "Test mapstore ado: \e[31mfail\e[0m" | tee -a /tmp/mcas/results.log
else
    echo -e "Test mapstore ado: \e[32mpassed\e[0m"| tee -a /tmp/mcas/results.log
fi
