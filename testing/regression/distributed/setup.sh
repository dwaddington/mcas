#!/bin/bash

cd /tmp/mcas
rm -rf build
mkdir build
cd build
cmake -DBUILD_KERNEL_SUPPORT=1 -DFLATBUFFERS_BUILD_TESTS=0 -DTBB_BUILD_TESTS=0 -DBUILD_PYTHON_SUPPORT=1 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/dist ..
make bootstrap
make install
