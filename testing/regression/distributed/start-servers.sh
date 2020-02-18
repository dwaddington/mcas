#!/bin/bash

folder=/tmp/mcas/build
cd $folder
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/dist/lib

if [[ -z $(lsmod | grep xpmem) ]]; then
    sudo insmod $folder/dist/lib/modules/*/*.ko
fi
#hstore and mapstore should use different ports from running instances as well, recommend to change according to play/conductor ports
./dist/bin/mcas --conf ../testing/regression/distributed/mapstore.conf &> /tmp/mcas/testmapstore-server.log &
USE_DRAM=24 ./dist/bin/mcas --conf ../testing/regression/distributed/hstore.conf &> /tmp/mcas/testhstore-server.log &
