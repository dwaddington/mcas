#!/bin/bash
#
# Packages for Ubuntu 18.04 LTS
apt-get update
apt-get install -y --no-install-recommends build-essential cmake

./install-tzdata-noninteractive.sh

# install Rust compiler and runtime
./install-rust.sh

# removed: asciidoc xmlto (needed only to build doc for ndctl)
# removed: build-essential (information for Debian packaging)
# removed: fabric (remote Python deployment)
# removed: doxygen libcunit1 libcunit1-dev liblz4-dev libomp-dev libsnappy-dev sloccount uuid (unused)
# removed: g++-multilib (cross-compiles)
# removed: google-perftools (profiling)
# removed: libpcap-dev (debugging)
apt-get install -y --no-install-recommends \
        autoconf automake ca-certificates cmake gcc g++ git make python3 python3-numpy libtool-bin pkg-config \
        libnuma-dev \
        libboost-system-dev libboost-iostreams-dev libboost-program-options-dev \
        libboost-filesystem-dev libboost-date-time-dev \
        libaio-dev libssl-dev libibverbs-dev librdmacm-dev \
        libudev-dev \
        libboost-python-dev libkmod-dev libjson-c-dev libbz2-dev \
        libelf-dev \
        libgtest-dev \
        libgoogle-perftools-dev libcurl4-openssl-dev \
        linux-headers-generic \
        uuid-dev golang gnutls-dev libgnutls30 \
        lcov libzmq5-dev libczmq-dev \
        python3-setuptools

# special handling of googletest
#
cd /usr/src/googletest/googletest
mkdir build
( cd build
  cmake ..
  make
  cp libgtest* /usr/lib/
)
rm -rf build
mkdir /usr/local/lib/googletest
ln -s /usr/lib/libgtest.a /usr/local/lib/googletest/libgtest.a
ln -s /usr/lib/libgtest_main.a /usr/local/lib/googletest/libgtest_main.a


