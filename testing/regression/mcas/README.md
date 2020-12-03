# Regression Tests

This directory contains a set of scripts for regresssion tests.  Regression
tests our run on an IBM internal server in response to github Pull Requests.

The tests are run locally with the following command from the build directory.

./dist/testing/run-tests.sh

To run tests with release build do:

./dist/testing/run-tests.sh release



They can also be run individually. For example,

./dist/testing/mcas-hstore-basic-0.sh


NOTE: Be careful not to interept tests and leave processes hanging.


Manual Running Examples
--------------------

gdb --args ./dist/bin/mcas --config '{"shards": [{"core": 0, "addr": "10.0.0.101", "port": 11911, "net": "mlx5_0", "default_backend": "mapstore"}]}' --forced-exit --debug 2
gdb --args ./dist/bin/kvstore-perf --cores 14 --src_addr 10.0.0.101 --server 10.0.0.101 --test put --component mcas --elements 2000000 --size 400000000 --skip_json_reporting --key_length 8 --value_length 8 --debug_level 2
