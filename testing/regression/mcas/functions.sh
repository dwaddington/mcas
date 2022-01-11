

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
	typeset -ir color=$1
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
    typeset -r log=$1
    typeset -r testid=$2
    if cat $log | grep -q 'FAILED TEST' ; then
        echo "Test $testid: $(color_fail fail)"
    elif cat $log | grep -q 'PASSED' ; then
        echo "Test $testid: $(color_pass passed)"
    else
        echo "Test $testid: $(color_fail fail) (no results)"
    fi
}

pass_by_iops () {
    typeset -r log=$1
    shift
    typeset -r testid=$1
    shift
    typeset -ir goal=$(scale_by_transport "$@")

    iops=$(cat $log | grep -Po 'IOPS: \K[0-9]*')

    exception=$(cat $log | grep 'exception' | head -1)

    if [ -n "$exception" ]; then
        echo "Test $testid: $(color_fail fail) ($exception)"
    elif [ -z "$iops" ]; then
        echo "Test $testid: $(color_fail fail) (no data)"
    elif [ "$iops" -lt $goal ]; then
        echo "Test $testid: $(color_fail fail) ($iops of $goal IOPS)"
    else
        echo "Test $testid: $(color_pass passed) ($iops of $goal IOPS)"
    fi
}

# two possibilities: have mlx5 and use RDMA, or don't have it and use sockets.
# sockets are 1000x slower
# This scales down only, not up
# scaling up would be easier and lead to smaller input numbers, but the
# historical input numbers are for mlx5, and therefore large numbers.
scale_by_transport () {
 typeset -ir base=$1
 # in presence of a second parameter, scale down by that parameter
 factor=${2:-1}
 # in absence of an mlx5 adaptor, scale down by 1000
 if ! has_mlx5
 then factor=1000
 fi
 echo $(( (base + factor - 1) / factor ))
}

find_devdax () {
 for i in 0 1
 do
  for j in $(seq 0 15)
  do
   d=/dev/dax$i
   if test -c "$d.$j"
   then echo $d
     return
   fi
  done
 done
}

find_fsdax () {
 for i in 0 1
 do
  d=/mnt/pmem$i
  if findmnt $d 2>/dev/null -a test -w $d
  then echo $d
   return
  fi
 done
}

has_module() {
 /usr/sbin/lsmod | grep "^$1 " &> /dev/null
}

has_module_mcasmod () {
 has_module mcasmod
}

has_module_xpmem () {
 has_module xpmem
}

# Decide whether to use device DAX or FS DAX, depending on whether this system has devdax configured
choose_dax() {
 t="${DAX_PREFIX:-}"
 if test -z "$t"
 then t="$(find_devdax)"
 fi
 if test -z "$t"
 then t="$(find_fsdax)"
 fi
 echo $t
}

dax_type() {
 case "$1" in
  /mnt*) echo "fsdax"
  ;;
  /dev/dax*) echo "devdax"
  ;;
  *) echo "?"
  esac
}

# determine numa node from DAX_PREFIX (arg 1): the numeric suffix of the DAX prefix.
numa_node() {
 echo "$1" | egrep -o '[0-9]+$'
}

# Pick a CPU number to use, but not larger than the max CPU number on this system
clamp_cpu () {
 typeset -ir cpu_desired=$1
 typeset -ir cpu_max=$(($(/bin/grep ^processor /proc/cpuinfo | wc -l) - 1))
 echo $((cpu_desired < cpu_max ? cpu_desired : cpu_max))
}

# scale the first input by percentages represented by subsequent inputs
scale() {
  typeset -ir sf=900 # (2*3*5)^2, an attempt to lessen rounding effects
  typeset -i base=$(($1*sf))
  shift
  for i in $@
  do base=$((base*i/100))
  done
  echo $((base/sf))
}
