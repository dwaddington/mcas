#!/bin/bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib
NODE_IP=$(ip addr | grep -Po 'inet \K10.0.0.[0-9]+' | head -1)
TESTID=$(basename --suffix .sh -- $0)
DESC=$TESTID

# launch MCAS server
DAX_RESET=1 ./dist/bin/mcas --conf ./dist/testing/hstore-ado0.conf --debug 0 &> test$TESTID-server.log &
SERVER_PID=$!

# give time to start server
sleep 3

CLIENT_LOG="test$TESTID-client.log"
#launch client
./dist/bin/ado-test --port 11911 --server $NODE_IP &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

if cat $CLIENT_LOG | grep -q 'FAILED TEST' ; then
    echo -e "Test $TESTID ($DESC): \e[31mfail\e[0m"
else
    echo -e "Test $TESTID ($DESC): \e[32mpassed\e[0m"
fi
