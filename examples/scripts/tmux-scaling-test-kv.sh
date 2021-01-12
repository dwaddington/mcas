#!/bin/bash
#
#  Copyright [2020] [IBM Corporation]
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#      http://www.apache.org/licenses/LICENSE-2.0
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#
#  This script is an example used for testing scaling of basic key-value
#  performance.  The script should be run from the server and it will connect
#  to the client machines.  Use ssh-copy-id to copy SSH credentials.
#
#  tmux-scaling-test.sh <shard count> <client-host-1> <client-host-2> <client-host-3> <client-host-4>
# 

if [ "$#" -lt 1 ]; then
	echo "$0 <shard_count> <client-host-1> <client-host-2> <client-host-3> <client-host-4>"
	exit 0
fi

# script assumes build path same on all client machines
MCAS_BUILD_DIR=$HOME/mcas/build

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${MCAS_BUILD_DIR}/dist/lib

LAUNCH="tmux split-window -d "
declare CLIENT_HOST=()

SHARD_COUNT=$1

# client host names are passed as script parameters
CLIENT_HOST[0]=$2
CLIENT_HOST[1]=$3
CLIENT_HOST[2]=$4
CLIENT_HOST[3]=$5

SERVER_HOST=$(ip addr | grep -Po 'inet \K10.0.0.[0-9]+' | head -1)


# add -d to not make new window current window

tmux bind -n C-e select-pane -t :.+
tmux set-option remain-on-exit off
#tmux set-environment USE_GDB 1
#tmux set-environment USE_XTERM 1
tmux set-environment DAX_RESET 1
tmux select-layout tiled

# launch MCAS shard process
#
declare SERVER_PANES=()


# local launch single-shard server: <pane-id> <port>
local_launch_server() {
    port=$((11900 + $1))
    loadbase=$((0x900000000 + ($1 * 0x100000000)))
    echo "../tools/gen-config.py --loadbase ${loadbase} --port ${port} --shards 1 --core $1 --path /dev/dax0.$1"
    # make sure dax device is on the same PCI domain as RNIC
    ../tools/gen-config.py --loadbase ${loadbase} --port ${port} --shards 1 --core $2 --path /dev/dax1.$1 > tmp-$1.cfg
    ${LAUNCH} -v ${MCAS_BUILD_DIR}/dist/bin/mcas --config tmp-$1.cfg --debug 0 
    SERVER_PANES[$1]=$(tmux list-panes -F '#{pane_id}' | sort -n --key=1.2 | tail -1)
    tmux select-layout tiled
#    sleep 3
}

declare CLIENT_PANES=()
# --port_increment 6
# 
# remove_launch_client 
remote_launch_client() {

    # now launch client
	${LAUNCH} -v ssh ${CLIENT_HOST[$2]} LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${MCAS_BUILD_DIR}/dist/lib \
	          ${MCAS_BUILD_DIR}/dist/bin/kvstore-perf --cores $3 --port $((11900 + $1)) --server $SERVER_HOST \
            --size 10485760 \
            --skip_json_reporting \
            --duration 60 --test throughput --read_pct 100 --log /tmp/exp-throughput-log-$1.txt\
            --value_length 16
	CLIENT_PANES[$1]=$(tmux list-panes -F '#{pane_id}' | sort -n --key=1.2 | tail -1)
	tmux select-layout tiled
}

SHARDS=0
get_client_log() {
    if (( $SHARDS < $SHARD_COUNT )) ; then
        X=`ssh ${CLIENT_HOST[$2]} cat /tmp/exp-throughput-log-$1.txt`
        Y=`echo $X | awk '{ print $3 }'`
        echo "Adding ($Y) to $RESULT.."
        RESULT=$(($RESULT + $Y))
        SHARDS=$(($SHARDS + 1))
    fi
}

# launch servers
# make sure to use cores on the socket corresponding
# to the affinity of RNIC and devdax.
# these cores are for aep2a (socket 1)
#
local_launch_server 0 16

if (( $SHARD_COUNT > 1 )) ; then
    local_launch_server 1 17
fi
if (( $SHARD_COUNT > 2 )) ; then
    local_launch_server 2 18
fi
if (( $SHARD_COUNT > 3 )) ; then
    local_launch_server 3 19
fi
if (( $SHARD_COUNT > 4 )) ; then
    local_launch_server 4 20
fi
if (( $SHARD_COUNT > 5 )) ; then    
    local_launch_server 5 21
fi
if (( $SHARD_COUNT > 6 )) ; then    
    local_launch_server 6 22
fi
if (( $SHARD_COUNT > 7 )) ; then    
    local_launch_server 7 23
fi
if (( $SHARD_COUNT > 8 )) ; then    
    local_launch_server 8 24
fi
if (( $SHARD_COUNT > 9 )) ; then    
    local_launch_server 9 25
fi
if (( $SHARD_COUNT > 10 )) ; then    
    local_launch_server 10 26
fi
if (( $SHARD_COUNT > 11 )) ; then    
    local_launch_server 11 27
fi
if (( $SHARD_COUNT > 12 )) ; then    
    local_launch_server 12 28
fi
if (( $SHARD_COUNT > 13 )) ; then    
    local_launch_server 13 29
fi
if (( $SHARD_COUNT > 14 )) ; then    
    local_launch_server 14 30
fi
if (( $SHARD_COUNT > 15 )) ; then    
    local_launch_server 15 31
fi

# make sure servers are ready
sleep 10

# launch clients
#
remote_launch_client 0 0 0-4

if (( $SHARD_COUNT > 1 )) ; then
    remote_launch_client 1 0 5-9
fi
if (( $SHARD_COUNT > 2 )) ; then
    remote_launch_client 2 0 10-14
fi
if (( $SHARD_COUNT > 3 )) ; then
    remote_launch_client 3 0 15-19
fi
if (( $SHARD_COUNT > 4 )) ; then
    remote_launch_client 4 1 0-4
fi
if (( $SHARD_COUNT > 5 )) ; then    
    remote_launch_client 5 1 5-9
fi
if (( $SHARD_COUNT > 6 )) ; then    
    remote_launch_client 6 1 10-14
fi
if (( $SHARD_COUNT > 7 )) ; then    
    remote_launch_client 7 1 15-19
fi
if (( $SHARD_COUNT > 8 )) ; then    
    remote_launch_client 8  2 0-4
fi
if (( $SHARD_COUNT > 9 )) ; then    
    remote_launch_client 9  2 5-9
fi
if (( $SHARD_COUNT > 10 )) ; then    
    remote_launch_client 10 2 10-14
fi
if (( $SHARD_COUNT > 11 )) ; then    
    remote_launch_client 11 2 15-19
fi
if (( $SHARD_COUNT > 12 )) ; then    
    remote_launch_client 12 3 0-4
fi
if (( $SHARD_COUNT > 13 )) ; then    
    remote_launch_client 13 3 5-9
fi
if (( $SHARD_COUNT > 14 )) ; then    
    remote_launch_client 14 3 10-14
fi
if (( $SHARD_COUNT > 15 )) ; then    
    remote_launch_client 15 3 15-19
fi


# pause
read -p "Press enter to get client logs .."

# get_client <pane-id> <host-index> >
get_client_log 0 0
get_client_log 1 0
get_client_log 2 0
get_client_log 3 0

get_client_log 4 1
get_client_log 5 1
get_client_log 6 1
get_client_log 7 1

get_client_log 8  2
get_client_log 9  2
get_client_log 10 2
get_client_log 11 2

get_client_log 12 3
get_client_log 13 3
get_client_log 14 3
get_client_log 15 3

echo "Total: $RESULT ($SHARD_COUNT shards)"

read -p "Press enter to continue"

killall -9 mcas ado

# clean up servers
for var in "${CLIENT_PANES[@]}"
do
  #echo "${var}"
  tmux send-keys -t ${CLIENT_PANES[$var]} C-c ENTER q ENTER exit ENTER
done

# clean up servers
for var in "${SERVER_PANES[@]}"
do
  #echo "${var}"
  tmux send-keys -t ${SERVER_PANES[$var]} C-c ENTER q ENTER exit ENTER
done

