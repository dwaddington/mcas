# Simple Graph Example

This example demonstrates how graph data can be managed in MCAS.

## Running Test

MCAS server:
```
USE_XTERM=1 USE_GDB=1 ./dist/bin/mcas --conf ./dist/conf/finex-example.conf --debug 0
```

Client:

```
./dist/bin/personality-cpp-list-test --server <ipaddress of server> --datadir ./dist/data/finance-graph
```

