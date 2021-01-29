# C++ std::list Example

This example personality shows a very basic version of moving C++ std
data structures from the client local memory space to the MCAS server.
Once in the server, additional operations can be performed.  Note,
this example is NOT crash-consistent.  To do this you will need to use
a crash-consistent std data structure.  This example uses the mapstore
backend.

## Running Test

MCAS server:
```
./dist/bin/mcas --conf ./dist/conf/cpp-list.conf --debug 0 --forced-exit
```

Note: we use --force-exit to exit the shard and clear data between runs.

To view the ADO process in a separate window:
```
USE_XTERM=1 USE_GDB=1 ./dist/bin/mcas --conf ./dist/conf/cpp-list.conf --debug 3
```

Client:

```
./dist/bin/personality-cpp-list-test --server <server-ip-address>
```

  
