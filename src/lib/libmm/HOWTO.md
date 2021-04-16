
# Basic Use

Use LD_PRELOAD to overload symbols:

LD_PRELOAD=dist/lib/libmm.so src/lib/libmm/mmwrapper-test-prog

Add LD_DEBUG=all to check out loading sequence.

# Debugging with GDB

```
gdb -ex 'set env LD_PRELOAD dist/lib/libmm.so' src/lib/libmm/mmwrapper-test-prog
```