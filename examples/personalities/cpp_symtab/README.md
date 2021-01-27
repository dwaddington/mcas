# C++ symbol table example

This example personality shows how a symbol table with bidirectional
lookup can be realized and exposed to the ADO.  This example is
not fully crash consistent.

## Running Test

MCAS server:

```bash
./dist/bin/mcas --conf ./dist/conf/cpp-symtab.conf --debug 3
```

To launch the ADO in a separate window:

```bash
USE_XTERM=1 USE_GDB=1 ./dist/bin/mcas --conf ./dist/conf/cpp-symtab.conf --debug 3
```

Client:

```
./dist/bin/personality-cpp-symtab-test --data ./dist/data/words/google-10000-english.txt --server <server-ip-addr>
```

  
