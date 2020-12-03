# ADO Test

ado-test is a testing client for the libcomponent-adoplugin-testing.so plugin.  It is used
in the regression tests.

Server side:

``` bash
./dist/bin/mcas --conf ./dist/conf/example-mapstore-adotest0.conf --debug 3
```

To list tests:

``` bash
./dist/bin/ado-test --server 10.0.0.101 --gtest_list_tests
```

To run specific test (e.g. BaseAddr):

``` bash
/dist/bin/ado-test --server 10.0.0.101 --gtest_filter=*BaseAddr
```
