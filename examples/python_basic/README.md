# Basic Python API example

This example demonstrates the MCAS client API for Python.

## Create Configuration File

For example, using helper script:

``` bash
./dist/testing/mapstore-0.py 10.0.0.101 > myConfig.conf
```

## Running Test

Server: 

``` bash
./dist/bin/mcas --conf myConfig.conf --debug 0
```

Client:

``` bash
./dist/bin/example-python-basic.py 10.0.0.101
```

