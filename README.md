# MCAS

Memory Centric Active Storage is a high-performance key-value store designed for persistent memory storage.

The key attributes of the solution are:

1. High-performance key-value store based on hash-table primary index and optional secondary indices.
2. Support for Intel Optane DC Persistent Memory or conventional DRAM (without persistence).
3. Support for both RDMA and traditional TCP/IP network transports.
4. Zero-copy transfer capable with RDMA and GPU-Direct capable.
5. Support for C++ and Python clients. 

## Documentation

* [Quick Start](./info/quick_start.md)
* [Overview](./info/MCAS_overview.md)
* [More documentation](./info/index.md)

## How to Build

Check out source (for example public version):

``` bash
git clone https://github.com/IBM/mcas.git
```

### update submodules
```bash
cd mcas
git submodule update --init --recursive
```

### configure

Create build directory at root level.  We normally use `mcas/build`:

```bash
mkdir build
cd build
cmake -DBUILD_KERNEL_SUPPORT=1 -DFLATBUFFERS_BUILD_TESTS=0 -DTBB_BUILD_TESTS=0 -DBUILD_PYTHON_SUPPORT=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/dist ..
```

Sometimes we build with an alternate compiler:

```bash
mkdir clang
cd clang
cmake -DBUILD_KERNEL_SUPPORT=1 -DFLATBUFFERS_BUILD_TESTS=0 -DTBB_BUILD_TESTS=0 -DBUILD_PYTHON_SUPPORT=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/dist ..
```

Or with code coverage:

```bash
mkdir coverage
cd coverage
cmake -DBUILD_KERNEL_SUPPORT=1 -DFLATBUFFERS_BUILD_TESTS=0 -DTBB_BUILD_TESTS=0 -DBUILD_PYTHON_SUPPORT=1 -DCMAKE_BUILD_TYPE=Debug -DCODE_COVERAGE=1 -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/dist ..
```

### one-time build
```bash
make bootstrap
```

### normal build
```bash
make -j
```


