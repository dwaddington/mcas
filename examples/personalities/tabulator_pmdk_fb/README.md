# Tabulator Example

This example demonstrates the use of PMDK/fsdax in the ADO plugin.  It keeps
min, max, mean for ongoing puts of a numerical value.  The client is based
on Python and flatbuffers is used for the protocol.

## Configuration File

An example configuration file is given in dist/conf/tabulator.conf.  This 
configuration file uses fsdax (/mnt/pmem0).  Copy and edit if desired.

## Running Test

Server:

``` bash
USE_ODP=1 ./dist/bin/mcas --conf ./dist/conf/tabulator.conf
```

Client:

``` bash
./dist/bin/tabulator_test.sh 10.0.0.101
```


