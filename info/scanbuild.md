# Scan-build

Scan-build is the Clang static analyzer.

To list checkers:

```
scan-build --help-checkers
```

To run checks (from build directory):

```
make clean
scan-build -o output make -j
```

or use rule ${PROJECT_NAME}-check, e.g.

```
make clean
make mcas-check
```


