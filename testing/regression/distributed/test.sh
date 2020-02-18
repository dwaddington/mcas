#!/bin/bash

server=$1
client=$2
folder=$3

rsync -av $folder $server:/tmp/.
rsync -av $folder $client:/tmp/.

ssh $server 'cd /tmp/mcas/testing/regression/distributed && player server.cfg' &
ssh $client 'cd /tmp/mcas/testing/regression/distributed && player client.cfg' &

sleep 10

conduct conductor.cfg

ssh $server 'killall player'
ssh $client 'killall player'
