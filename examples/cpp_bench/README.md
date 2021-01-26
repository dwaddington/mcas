# Basic C++-client Benchmark

This example is a basic benchmark test using C++ API.

``` bash
../tools/gen-config.py --backend mapstore > myConfig.conf

./dist/bin/mcas --conf myConfig.conf --debug 0
```

Client example:

``` bash
./dist/bin/example-cpp-bench --server 10.0.0.101:11911 --pairs 10000  --basecore 9 --test write
```


