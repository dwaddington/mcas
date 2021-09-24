#!/bin/bash
#
# PLEASE RUN THIS AS NON-ROOT
#
# PP uses .site local installs
#
# Python3 should be installed
#
PIP=python3 -m pip
${PIP} install matplotlib --user -I
${PIP} install scikit-image --user -I
${PIP} install torch --user -I
${PIP} install torchvision --user -I
${PIP} install flatbuffers --user -I
${PIP} install parallel_sort --user -I
${PIP} install cython --user -I
${PIP} install chardet --user -I

# we use a custom version of numpy that allows overloading of memory allocators
# this is temporary until new features enter the mainline

wget https://github.com/dwaddington/python-wheels/archive/refs/tags/v1.0.tar.gz
tar -zxvf v1.0.tar.gz
${PIP} install ./python-wheels-1.0/numpy-1.19.6.dev0+78b5f9b-cp36-cp36m-linux_x86_64.whl --user -I
rm v1.0.tar.gz
rm -Rf ./python-wheels-1.0
