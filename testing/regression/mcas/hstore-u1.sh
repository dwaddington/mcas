#!/bin/bash

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. "$DIR/functions.sh"

#
# Test hstore. Not really an mcas test, but left in mcas until we find a better place for it.
#

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib
TESTID=$(basename --suffix .sh -- $0)

# launch hstore unit test 1
TEST_LOG="test$TESTID-test.log"
./src/components/store/hstore/unit_test/hstore-test1 &> $TEST_LOG &
TEST_PID=$!

# arm cleanup
trap "set +e; kill -s KILL $TEST_PID &> /dev/null" EXIT

# wait for client to complete
wait $TEST_PID

pass_fail $CLIENT_LOG $TESTID
