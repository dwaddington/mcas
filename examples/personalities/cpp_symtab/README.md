# C++ symbol table example

This example personality shows how a symbol table with bidirectional
lookup can be realized and exposed to the ADO.  This example is
crash consistent.

## Running Test

MCAS server:
```
USE_XTERM=1 USE_GDB=1 ./dist/bin/mcas --conf ./dist/conf/cpp-symtab.conf --debug 3
```

Client:

```
./dist/bin/personality-cpp-symtab-test --data ./dist/data/words/google-10000-english.txt --server <ipaddress of server>
```

  
