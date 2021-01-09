#!/bin/bash

#
# TMUX-TEST-HSTORE-SCALING.sh <client-host-1> <client-host-2> <client-host-3> <client-host-4>
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
CLIENT_HOST[0]=$2
CLIENT_HOST[1]=$3
CLIENT_HOST[2]=$4
CLIENT_HOST[3]=$5

SERVER_HOST=$(ip addr | grep -Po 'inet \K10.0.0.[0-9]+' | head -1)


# add -d to not make new window current window

tmux bind -n C-e select-pane -t :.+

#${LAUNCH} -v ssh ${CLIENT_HOST} -t 'htop'
#HTOP_PANE_ID=$(tmux list-panes -F '#{pane_id}' | sort -n --key=1.2 | tail -1)
#tmux select-pane -t $HTOP_PANE_ID -T "Client HTOP" #-P 'fg=black,bg=white'


#tmux set-environment USE_GDB 1
tmux set-environment DAX_RESET 1
tmux select-layout tiled

# launch MCAS shard process
#
CONFIG_FILE=${MCAS_BUILD_DIR}/dist/conf/example-hstore-devdax-scale-0.conf
#./dist/testing/hstore-devdax-0.py ${SERVER_HOST} > $CONFIG_FILE
#${LAUNCH} -v gdb -q --ex "catch signal SIGABRT" --ex r --args ${MCAS_BUILD_DIR}/dist/bin/mcas --conf ${CONFIG_FILE} --debug 0 &
${LAUNCH} -v ${MCAS_BUILD_DIR}/dist/bin/mcas --conf ${CONFIG_FILE} --debug 0 &
SHARD_PANE_ID=$(tmux list-panes -F '#{pane_id}' | sort -n --key=1.2 | tail -1)
tmux select-layout tiled

sleep 10

declare CLIENT_PANES=()

# --port_increment 6
# 
# remove_launch_client 
remote_launch_client() {
	${LAUNCH} -v ssh ${CLIENT_HOST[$2]} LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${MCAS_BUILD_DIR}/dist/lib \
	          ${MCAS_BUILD_DIR}/dist/bin/kvstore-perf --cores $3 --port $((11900 + $1)) --server $SERVER_HOST \
            --skip_json_reporting \
            --duration 60 --test throughput --read_pct 100 --log /tmp/exp-throughput-log-$1.txt\
            --value_length 16
	CLIENT_PANES[$1]=$(tmux list-panes -F '#{pane_id}' | sort -n --key=1.2 | tail -1)
	tmux select-layout tiled
	#sleep 3	
}

SHARDS=0
get_client_log() {
    if (( $SHARDS == $SHARD_COUNT )) ; then
        return
    fi
    X=`ssh ${CLIENT_HOST[$2]} cat /tmp/exp-throughput-log-$1.txt`
    Y=`echo $X | awk '{ print $3 }'`
    RESULT=$(($RESULT + $Y))
    SHARDS=$(($SHARDS + 1))
}

kill_client() {
	ssh ${CLIENT_HOST} killall -9 ${MCAS_BUILD_DIR}/dist/bin/kvstore-perf
	tmux send-keys -t ${CLIENT_PANES[$1]} C-c ENTER q ENTER exit ENTER
}

# launch clients (same host)
#
# remote_launch_client <pane-id> <host-index> <core>



# fixed for sg-mcas105,sg-mcas106,sg-mcas107,
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

echo "Total:$RESULT ($SHARDS shards)"

read -p "Press enter to continue"


killall -9 mcas

#kill_client 0
#kill_client 1
#kill_client 2
#kill_client 3

#tmux send-keys -t $HTOP_PANE_ID q ENTER

