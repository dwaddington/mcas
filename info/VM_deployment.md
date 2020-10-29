# Virtual Machine Deployment

These instructions are for virtualization of MCAS in QEMU/Linux KVM.  This only works on the Intel platform with Optane DC Persistent Memory DIMMs.

## RDMA Networking

In order to take advantage of high-performance, zero-copy, RDMA networking, the RDMA adapters must be virtualized using SR-IOV (single-root input-output virtualization).

Note, the Mellanox OFED distribution should be installed on the corresponding system.  These instructions have been tested on OFED 4.5.1 (see ```ofed_info | grep MLNX_OFED```). 
 
#### Enable SR-IOV on adapters
```bash
mst start
/* the following /dev/mst/mtXXX may be different */
mlxconfig -d /dev/mst/mt4119_pciconf0 set SRIOV_EN=1 NUM_OF_VFS=5
mlxconfig -d /dev/mst/mt4119_pciconf0  q | grep SRIOV
```

Configure MAC addresses for logical cards:

```bash
#!/bin/bash
ip link set enp24s0 vf 0 mac 98:03:9b:96:00:00
ip link set enp24s0 vf 1 mac 98:03:9b:96:00:01
ip link set enp24s0 vf 2 mac 98:03:9b:96:00:02
ip link set enp24s0 vf 3 mac 98:03:9b:96:00:03
ip link set enp24s0 vf 4 mac 98:03:9b:96:00:04
```

To use the NIC cards with virtualization, using PCI pass-through, the adapters will need unbinding from the Mellanox drivers and binding to ```vfio-pci```.

```bash
echo 5 > /sys/class/net/enp24s0/device/mlx5_num_vfs
```

```bash
[root@aep1 build]# lspci | grep Mell
18:00.0 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5]
18:00.1 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5 Virtual Function]
18:00.2 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5 Virtual Function]
18:00.3 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5 Virtual Function]
18:00.4 Ethernet controller: Mellanox Technologies MT27800 Family [ConnectX-5 Virtual Function]
```

Unbind from existing driver:

```bash
echo 0000\:18\:00.1 > /sys/bus/pci/devices/0000\:18\:00.1/driver/unbind
echo 0000\:18\:00.2 > /sys/bus/pci/devices/0000\:18\:00.2/driver/unbind
echo 0000\:18\:00.3 > /sys/bus/pci/devices/0000\:18\:00.3/driver/unbind
echo 0000\:18\:00.4 > /sys/bus/pci/devices/0000\:18\:00.4/driver/unbind
echo 0000\:18\:00.5 > /sys/bus/pci/devices/0000\:18\:00.5/driver/unbind
```

Find vendor pair:

```bash
$ lspci -n | grep 18:00.1
18:00.1 0200: 15b3:1018
```

Bind to VFIO driver (e.g.):

```bash
modprobe vfio-pci
echo 15b3 1017 > /sys/bus/pci/drivers/vfio-pci/new_id
```

Next, set the GUID for each of the logical adapters.  Any unique GUID can be used.

```bash
echo 11:22:33:44:77:66:77:90 > /sys/class/infiniband/mlx5_0/device/sriov/0/node
echo 11:22:33:44:77:66:77:91 > /sys/class/infiniband/mlx5_0/device/sriov/1/node
echo 11:22:33:44:77:66:77:92 > /sys/class/infiniband/mlx5_0/device/sriov/2/node
echo 11:22:33:44:77:66:77:93 > /sys/class/infiniband/mlx5_0/device/sriov/3/node
echo 11:22:33:44:77:66:77:94 > /sys/class/infiniband/mlx5_0/device/sriov/4/node
```

Trouble shooting RDMA info at [Mellanox Documentation](https://community.mellanox.com/s/article/howto-enable--verify-and-troubleshoot-rdma#jive_content_id_For_Ubuntu_Installation)

## QEMU TAP Networking

QEMU's TAP networking allows a VM to be routable from an external host.  For MCAS, this is necessary so as to make the service accessible from other nodes.  Note, this is normally done on the non-RDMA NIC.

On the host side, a network bridge must be set up.  On Centos, this is done as following:

Edit files in /etc/sysconfig/network-scripts/ifcfg-eno1

```
[root@aep1 network-scripts]# cat ifcfg-eno1 
TYPE=Ethernet
PROXY_METHOD=none
BROWSER_ONLY=no
#BOOTPROTO=dhcp
DEFROUTE=yes
IPV4_FAILURE_FATAL=no
IPV6INIT=no
IPV6_AUTOCONF=yes
IPV6_DEFROUTE=yes
IPV6_FAILURE_FATAL=no
IPV6_ADDR_GEN_MODE=stable-privacy
NAME=eno1
UUID=edce5e07-04a2-33d3-bda5-fdc2ea2f54d9
ONBOOT=yes
AUTOCONNECT_PRIORITY=-999
DEVICE=eno1
BRIDGE=br0
```

Configure bridge:

```
[root@aep1 network-scripts]# cat ifcfg-br0 
DEVICE=br0
TYPE=Bridge
ONBOOT=yes
BOOTPROTO=dhcp
```

If successful, the br0 interface will get the IP address:

```
[root@aep1 network-scripts]# ifconfig br0
br0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet 9.1.75.42  netmask 255.255.252.0  broadcast 9.1.75.255
        inet6 fe80::a6bf:1ff:fe21:f5bc  prefixlen 64  scopeid 0x20<link>
        ether a4:bf:01:21:f5:bc  txqueuelen 1000  (Ethernet)
        RX packets 2739400  bytes 330601907 (315.2 MiB)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 133671  bytes 63178854 (60.2 MiB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```

On QEMU, the following command line options can be used:

```
-device e1000,netdev=net0,mac=a4:bf:01:21:f5:bd \
-netdev tap,id=net0,script=./qemu-ifup
```

Note: the MAC address defines the address to be used in guest.  This MAC should be registered with the network and able to get a DHCP response.  Here is the qemu-ifup file that is needed.

```
#!/bin/sh
set -x
switch=br0
if [ -n "$1" ];then
    #tunctl -u `whoami` -t $1
    ip tuntap add $1 mode tap user `whoami`
    ip link set $1 up
    sleep 0.5s
    #brctl addif $switch $1
    ip link set $1 master $switch
    exit 0
else
    echo "Error: no interface specified"
    exit 1
fi
```

If the network set up is successful, you should be able to ping a node on the local network and vice versa.

## Intel Optane DC PMM Setup

For MCAS, *device DAX* is used (as opposed to file DAX).  This is needed for RDMA into the persistent memory.  ```/dev/daxX.Y``` files should be accessible on the host side (```sudo chmod a+rwx /dev/dax*```).  The device DAX partitions should be created with 2M or 1G alignment.

In side the host, the default is for the exposed DIMMs to be configured with fsdax.  You will need to reconfigured the regions with device DAX as follows (from the guest side):

```bash
ndctl create-namespace -m devdax -e namespace0.0 --align 2M --force
```

Note: alignment defines how pages are mapped.

Do not try to destroy and re-create the namespaces.  This will not work.  If the above command is successful, you can check the region configuration as follows:

```
root@aep1-vm0:# ndctl list
{
  "dev":"namespace0.0",
  "mode":"devdax",
  "map":"dev",
  "size":760209211392,
  "uuid":"ff2f87e0-6ee4-41ff-94f8-09d02069daea",
  "chardev":"dax0.0",
  "numa_node":0
}
```

You will need to change the ```/dev/dax0.0``` permissions on the guest side (```chmod a+rwx /dev/dax0.0```).

#### Guest Kernel

The guest kernel must support ACPI NFIT.  This has been tested for kernel 4.15.18

```
CONFIG_ACPI_NFIT=y
CONFIG_LIBNVDIMM=y
CONFIG_BLK_DEV_PMEM=m
CONFIG_ND_BLK=m
CONFIG_ND_CLAIM=y
CONFIG_ND_BTT=m
CONFIG_BTT=y
CONFIG_ND_PFN=m
CONFIG_NVDIMM_PFN=y
CONFIG_NVDIMM_DAX=y
CONFIG_DAX=y
CONFIG_DEV_DAX=m
CONFIG_DEV_DAX_PMEM=m
CONFIG_NVMEM=y
```

## Launching QEMU

Here is an example command line configuration of QEMU 4.0.  Note, taskset is used to restrict threads to specific cores.  The NV-DIMM is passed as a memory-backed-file.

```
sudo taskset --cpu-list 28-31,84-87 qemu-system-x86_64 \
     -machine pc,nvdimm,accel=kvm \
     -m 4G,slots=2,maxmem=4T \
     -cpu host\
     -hda ./hdd.qcow2\
     -smp 8 \
     -enable-kvm \
     -device vfio-pci,host=18:00.1 \
     -kernel bzImage \
     -initrd initrd.img \
     -append 'root=/dev/sda2 console=ttyS0' \
     -object memory-backend-file,id=mem1,share=on,mem-path=/dev/dax1.0,size=720G,align=2M \
     -device nvdimm,id=nvdimm1,memdev=mem1 \
     -fsdev local,id=mdev,path=/opt/mcas,security_model=none -device virtio-9p-pci,fsdev=mdev,mount_tag=host \
     -nographic \
     -device e1000,netdev=net0,mac=a4:bf:01:21:f5:bd \
     -netdev tap,id=net0,script=./qemu-ifup
```	
