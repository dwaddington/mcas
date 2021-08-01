# C++ symbol table example

This is the benchmark for range index
The table reside in the ADO and the secondary index have a pointer to the table

## Running Test

MCAS server:

```bash
./dist/bin/mcas --conf ./dist/conf/cpp-rangeindex.conf
```

Client:

```
./dist/bin/personality-cpp-rangeindex-test --data ./dist/data/traces/traces.txt --server <server-ip-addr>
```

  
