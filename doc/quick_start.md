## Quick Start

The easiest way to get going is to use the DRAM based key-value engine, *mapstore*.  Mapstore is volatile and therefore data is lost after the MCAS process has been shutdown.

### System Configuration

Make sure you load the ```xpmem.ko``` kernel module.  This is built as part of the distribution (e.g., dist/lib/modules/4.18.19-100.fc27.x86_64/xpmem.ko)

```bash
insmod ./dist/lib/modules/4.18.19-100.fc27.x86_64/xpmem.ko
```

### Launch MCAS server

The MCAS server can be launched from the build directory.  Using one of the pre-supplied (testing) configuration files:

```bash
./dist/bin/mcas --conf ./dist/testing/mapstore-0.conf
```

This configuration file defines a single shard, using port 11911 on the `mlx5_0` RDMA NIC adapter.

Note, ```./dist``` is the location of the installed distribution.

### Launch the Python client

Again, from the build directory:

```bash
./dist/bin/mcas-shell
```

First open a session to the MCAS server:

```python
session = mcas.Session(ip='10.0.0.101', port=11911)
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



