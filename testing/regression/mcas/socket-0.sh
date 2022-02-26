#!/bin/bash

#
# Test fabric sockets. Not really an mcas test, but left in mcas until we find a better place for it.
#

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

TESTID=$(basename --suffix .sh -- $0)

# parameters for MCAS server and client
NODE_IP="$(node_ip)"

# launch socket test server
./src/components/net/fabric/unit_test/fabric-test1 --gtest_filter=Fabric_test.WriteReadSequentialSockets &> test$TESTID-server.log &
SERVER_PID=$!

# give time to start server
sleep 3

CLIENT_LOG="test$TESTID-client.log"
# launch client
SERVER=$NODE_IP ./src/components/net/fabric/unit_test/fabric-test1 --gtest_filter=Fabric_test.WriteReadSequentialSockets &> $CLIENT_LOG &
CLIENT_PID=$!

# arm cleanup
trap "set +e; kill -s KILL $SERVER_PID $CLIENT_PID &> /dev/null" EXIT

# wait for client to complete
tail --pid=$CLIENT_PID -f /dev/null

if < $CLIENT_LOG fgrep '[       OK ] Fabric_test.WriteReadSequentialSockets' > /dev/null; then
    echo "Test $TESTID: $(color_pass passed)"
else
    echo "Test $TESTID: $(color_fail fail)"
fi
