#!/bin/bash

#
# Test hstore. Not really an mcas test, but left in mcas until we find a better place for it.
#

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib
TESTID=$(basename --suffix .sh -- $0)
DESC=$TESTID

# launch hstore unit test 1
TEST_LOG="test$TESTID-test.log"
./src/components/store/hstore/unit_test/hstore-test1 &> $TEST_LOG &
TEST_PID=$!

# arm cleanup
trap "kill -9 $TEST_PID &> /dev/null" EXIT

# wait for client to complete
wait $TEST_PID

if < $TEST_LOG fgrep '[  FAILED  ]' > /dev/null; then
    echo -e "Test $TESTID ($DESC): \e[31mfail\e[0m"
else
    echo -e "Test $TESTID ($DESC): \e[32mpassed\e[0m"
fi
