

# Find a private IP address to use for fabric verbs or sockets
node_10net_ip () {
    #    /sbin/ip addr | /bin/grep -Po 'inet \K(10\.[0-9]+|192\.168)\.[0-9]+\.[0-9]+' | /usr/bin/head -1`
    /sbin/ip addr | /bin/grep -Po 'inet \K10\.[0-9]+\.[0-9]+\.[0-9]+' | /usr/bin/head -1
}

node_192net_ip () {
    /sbin/ip addr | /bin/grep -Po 'inet \K(192\.168)\.[0-9]+\.[0-9]+' | /usr/bin/head -1
}

node_ip() {
    if ! node_10net_ip ;
    then
	node_192net_ip
    fi
}
    

has_mlx5 () {
 test -f /sys/class/infiniband/mlx5_0/ports/1/state
}

color_any () {
	color=$1
	shift
    echo -e "\e[${color}m$@\e[0m"
}

color_fail () {
    echo $(color_any 31 $@)
}

color_pass () {
    echo $(color_any 32 $@)
}

pass_fail_by_code () {
    while (( $# ))
    do  if [ $2 -ne 0 ]
        then echo "Test $(color_fail fail) $1 rc $2"
            exit 1
        fi
        shift; shift
    done
}

pass_fail () {
    typeset LOG=$1
    typeset TESTID=$2
    if cat $LOG | grep -q 'FAILED TEST' ; then
        echo "Test $TESTID: $(color_fail fail)"
    elif cat $LOG | grep -q 'PASSED' ; then
        echo "Test $TESTID: $(color_pass passed)"
    else
        echo "Test $TESTID: $(color_fail fail) (no results)"
    fi
}

pass_by_iops () {
    typeset LOG=$1
    shift
    typeset TESTID=$1
    shift
    typeset -i GOAL=$(scale_by_transport "$@")

    iops=$(cat $LOG | grep -Po 'IOPS: \K[0-9]*')

    exception=$(cat $LOG | grep 'exception' | head -1)

    if [ -n "$exception" ]; then
        echo "Test $TESTID: $(color_fail fail) ($exception)"
    elif [ -z "$iops" ]; then
        echo "Test $TESTID: $(color_fail fail) (no data)"
    elif [ "$iops" -lt $GOAL ]; then
        echo "Test $TESTID: $(color_fail fail) ($iops of $GOAL IOPS)"
    else
        echo "Test $TESTID: $(color_pass passed) ($iops of $GOAL IOPS)"
    fi
}

# two possibilities: have mlx5 and use RDMA, or don't have it and use sockets.
# sockets are 1000x slower
# This scales down only, not up
# scaling up would be easier and lead to smaller input numbers, but the
# historical input numbers are for mlx5, and therefore large numbers.
scale_by_transport () {
 typeset -i base=$1
 # in presence of a second parameter, scale down by that parameter
 factor=${2:-1}
 # in absence of an mlx5 adaptor, scale down by 1000
 if ! has_mlx5
 then factor=1000
 fi
 echo $(( (base + factor - 1) / factor ))
}

has_devdax () {
 test -c /dev/dax0.0
}

has_fsdax () {
 test -d /mnt/pmem1
}

has_module_mcasmod () {
 /sbin/modinfo mcasmod &> /dev/null
}

has_module_xpmem () {
 /sbin/modinfo xpmem &> /dev/null
}

# Decide whether to use device DAX or FS DAX, depending on whether this system has devdax configured
choose_dax_type() {
 if [[ -n "$DAXTYPE" ]]
 then
  echo $DAXTYPE
 else
  if has_devdax
  then echo devdax
  else echo fsdax
  fi
 fi
}

# Pick a CPU number to use, but not larger than the max CPU number on this system
clamp_cpu () {
 typeset -i CPU_DESIRED=$1
 typeset -i CPU_MAX=$(($(/bin/grep ^processor /proc/cpuinfo | wc -l) - 1))
 echo $((CPU_DESIRED < CPU_MAX ? CPU_DESIRED : CPU_MAX))
}

# scale the first input by percentages represented by subsequent inputs
scale() {
  typeset -i sf=900 # (2*3*5)^2, an attempt to lessen rounding effects
  base=$(($1*sf))
  shift
  for i in $@
  do base=$((base*i/100))
  done
  echo $((base/sf))
}
