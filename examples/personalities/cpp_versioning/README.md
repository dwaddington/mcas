# C++ Key-Value Versioning Example

This example personality shows how the ADO mechanism can be used to layer
additional services (such as versioning) over the basic key-value store.

This example uses hstore with fsdax (/mnt/pmem0) and uses a flatbuffer based
protocol.

## Running Test

MCAS server (edit configuration file as appropriate):
```
USE_ODP=1 ./dist/bin/mcas --conf ./dist/conf/example-versioning.conf --debug 0
```

To view the ADO process in a separate window:
```
USE_ODP=1 USE_XTERM=1 USE_GDB=1 ./dist/bin/mcas --conf ./dist/conf/example-versioning.conf --debug 0
```

Client:

```bash
./dist/bin/personality-example-versioning-test --server <server-ip-addr> 
```

