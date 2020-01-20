#!/bin/bash

#
# Test fabric sockets. Not really an mcas test, but left in mcas until we find a better place for it.
#

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib
NODE_IP=$(ip addr | grep -Po 'inet \K10.0.0.[0-9]+' | head -1)
TESTID=$(basename --suffix .sh -- $0)
DESC=$TESTID

# launch socket test server
./src/components/net/fabric/unit_test/fabric-test1 --gtest_filter=Fabric_test.WriteReadSequentialSockets &> test$TESTID-server.log &
SERVER_PID=$!

# give time to start server
sleep 3

CLIENT_LOG="test$TESTID-client.log"
#launch client
SERVER=$NODE_IP ./src/components/net/fabric/unit_test/fabric-test1 --gtest_filter=Fabric_test.WriteReadSequentialSockets &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "kill -9 $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

if < $CLIENT_LOG fgrep '[       OK ] Fabric_test.WriteReadSequentialSockets' > /dev/null; then
    echo -e "Test $TESTID ($DESC): \e[32mpassed\e[0m"
else
    echo -e "Test $TESTID ($DESC): \e[31mfail\e[0m"
fi
