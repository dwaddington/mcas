#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# run each test
$DIR/mcas-mapstore-basic-0.sh
echo "mcas-mapstore-basic-0 complete."
sleep 5
$DIR/mcas-hstore-basic-0.sh
echo "mcas-hstore-basic-0 complete."
sleep 5
$DIR/mcas-hstore-cc-basic-0.sh
echo "mcas-hstore-cc-basic-0 complete."
sleep 5
$DIR/mcas-hstore-dram-0.sh
echo "mcas-hstore-dram-0 complete."
sleep 5
$DIR/mcas-hstore-ado-0.sh
echo "mcas-hstore-ado-0 complete."
sleep 5
$DIR/mcas-mapstore-ado-0.sh
echo "mcas-mapstore-ado-0 complete."
sleep 5
