# Basic C++ benchmark example

This example demonstrates the basic key-value API for C++.

## Create Configuration File

For example, using helper script:

``` bash
./dist/testing/mapstore-0.py 10.0.0.101 > myConfig.conf
```

## Running Test

``` bash
./dist/bin/mcas --conf myConfig.conf --debug 0
```

Client:

``` bash
./dist/bin/example-cpp-bench --server 10.0.0.101:11911 --debug 0
```


