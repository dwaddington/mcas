# Tabulator Example

This example demonstrates the use of CCPM (crash-consistent C++
templates) in the ADO plugin.  It keeps min, max, mean for ongoing
puts of a numerical value.  The client is based on Python and
flatbuffers is used for the protocol.

## Configuration File

An example configuration file is given in dist/conf/tabulator-ccpm.conf.  This 
configuration file uses fsdax (/mnt/pmem0).  Copy and edit if desired.

## Running Test

Server: (we use DAX_RESET=1 to force clearing of memory)

``` bash
DAX_RESET=1 USE_ODP=1 ./dist/bin/mcas --conf ./dist/conf/tabulator-ccpm.conf
```

Client:

``` bash
./dist/bin/tabulator_test.sh 10.0.0.101
```

Output:

``` python
Status:  0 
Min-> 1.0 
Max-> 5.0 
Mean-> 2.0 
```

