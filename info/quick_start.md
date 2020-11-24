## Quick Start

The easiest way to get going is to use the DRAM based key-value
engine, *mapstore*.  Mapstore is volatile and therefore data is lost
after the MCAS process has been shutdown.  You can use plain TCP/IP sockets
if you don't have RDMA available.

## Supported Linux Distributions

MCAS is tested on the following distributions:

* Ubuntu 18.04 x86_64
* Fedora Core 27 and Fedora Core 30 x86_64

These distributed are supported by Mellanox OFED, which is required for RDMA.

### System Configuration

To build kernel modules cmake option BUILD_KERNEL_SUPPORT=1 is
required. The current implementation uses two kernel modules for
subspace mapping (sharing memory with the ADO).  The ```xpmem```
module is used with ```mapstore```.  This module supports arbitrary
memory but will not allow DMA from the ADO plugin.  For example, an
ADO plugin used with ```mapstore``` cannot itself use the MCAS
put_direct/get_direct client APIs (e.g. for replication).

Load the ```xpmem.ko``` kernel module as follows:

```bash
insmod ./dist/lib/modules/4.18.19-100.fc27.x86_64/xpmem.ko
```

Alternatively, if you are using the hstore backend with _devdax_ persistent
memory, then the ```mcasmod``` kernel module is needed.  This module
requires memory from either persistent memory DIMMs or simulated
memory established via the MEMMAP kernel boot option.

Again, you should load the kernel module as follows:

```bash
insmod ./dist/bin/mcasmod.ko
```

Note: both modules can be loaded into the system at the same time.

If you are using _fsdax_ persistent or simulated memory with ```hstore``` 
or ```hstore-cc```, then this kernel module is not required.

#### Create Configuration file

MCAS uses libfabric and therefore supports either RDMA or traditional TCP/IP networking.  Use
the following helper script to create your configuration file for the MCAS server process.

For RDMA/verbs networking:

```bash
cd ~/mcas/build/dist/testing
python mapstore-0.py <Your-IP-Address> > myConfig.conf
cd ../../../
```

For TCP/IP sockets networking:

```bash
cd ~/mcas/build/dist/testing
python mapstore-0-sockets.py <Your-IP-Address> <Your-Ethernet-Device(e.g. eth0)> > myConfig.conf
cd ../../../
```


### Launch MCAS server

The MCAS server can be launched from the build directory.  Using one of the pre-supplied (testing) configuration files:

```bash
./dist/bin/mcas --conf ./dist/testing/myConfig.conf
```

This configuration file defines a single shard, using port 11911 on the `mlx5_0` RDMA NIC adapter.

Note, ```./dist``` is the location of the installed distribution.

### Launch the Python client

Again, from the build directory:

```bash
./dist/bin/mcas-shell
```

First open a session to the MCAS server.

```python
session = mcas.Session(ip='10.0.0.101', port=11911)
```

If you are using TCP/IP sockets, you will need to additionally specify the network device as follows:

```python
session = mcas.Session(ip='10.0.0.201', device='eth0', port=11911)
```

Next create a pool. Provide pool name, size of pool in bytes and expected number of objects (presizes hash table):

```python
pool = session.create_pool('pool0', 64*1024, 1000)
```

Now we can create key-value pairs:

```python
pool.put('myPet','doggy')
```

And then retrieve the value back:

```python
pool.get('myPet')
```

We can configure a volatile index for the pool.  This allows us to perform scans on the key space - find_key(expression, offset).

```python
pool.configure("AddIndex::VolatileTree")
pool.find_key('regex:.*', 0)
```

Finally, the pool can be closed.

```python
pool.close()
```



