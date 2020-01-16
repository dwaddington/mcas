# MCAS

Memory Centric Active Storage is a high-performance key-value store designed for persistent memory storage.

The key attributes of the solution are:

1. High-performance key-value store based on hash-table primary index and optional secondary indices.
2. Support for Intel Optane DC Persistent Memory or conventional DRAM (without persistence).
3. Support for both RDMA and traditional TCP/IP network transports.
4. Zero-copy transfer capable with RDMA and GPU-Direct capable.
5. Support for C++ and Python clients. 

## Documentation

Work in progress.

* [Quick Start](./doc/quick_start.md)
* [Overview](./doc/MCAS_overview.md)
* [More documentation](./doc/index.md)

## How to Build

### update submodules
```bash
git submodule update --init --recursive
```

### configure
```bash
cmake -DBUILD_KERNEL_SUPPORT=1 -DFLATBUFFERS_BUILD_TESTS=0 -DTBB_BUILD_TESTS=0 -DBUILD_PYTHON_SUPPORT=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/dist ..
```

### one-time build
```bash
make bootstrap
```

### normal build
```bash
make -j
```


