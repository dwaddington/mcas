#!/bin/bash
#
# PP uses .site local installs
#
# Python3 should be installed
#
pip3 install matplotlib --user -I
pip3 install scikit-image --user -I
pip3 install torch --user -I
pip3 install torchvision --user -I
pip3 install flatbuffers --user -I
pip3 install parallel_sort --user -I

# we use a custom version of numpy that allows overloading of memory allocators
# this is temporary until new features enter the mainline

wget https://github.com/dwaddington/python-wheels/archive/refs/tags/v1.0.tar.gz
tar -zxvf v1.0.tar.gz
pip3 install ./python-wheels-1.0/numpy-1.19.6.dev0+78b5f9b-cp36-cp36m-linux_x86_64.whl --user -I
rm v1.0.tar.gz
rm -Rf ./python-wheels-1.0
