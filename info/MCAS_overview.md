## MCAS Architecture

MCAS uses a shared-nothing, sharded architecture.  Each shard, managed by a single thread without locking, represents a set of *pools*.  Currently, the mapping of pools to shards is governed by the client (e.g., by hashing of the pool name).

![MCAS Architecture](./figures/mcas-01.svg)

### Memory Pools

*Pools* are the fundamental unit of memory resource alllcation in the storage node.  Pools map to one or more contiguous regions of memory, which can be expanded on-demand.  Within an ADO context, pools also define the visibility boundary to the ADO process.

Pool memory is either Intel Optane DC Persistent Memory, DRAM or a conventional file (used mainly for debugging).  The type of memory is governed by the configured key-value engine.  For example, *hstore* uses persistent memory, *mapstore* uses DRAM and *filestore* uses POSIX files.  The key-value engine is set by the configuration file defined by the MCAS program command line parameters.  For *hstore* and *mapstore*, the total amount of memory available for the shard (i.e. for all pools in the shard) is statically configured.

### Key-Value Pairs

The memory-centric key-value engines, i.e., hstore and mapstore, use a hash-table to map keys to values.  There is no limit on the size of the key or the value.  However, for small keys and values, certain in-lining optimizations may occur.


### Shard Threads

Each shard thread is responsible for servicing connection requests on its corresponding TCP Port, handling session requests (e.g., put, get), and issuing responses to the client.  Under-the-hood, the shard thread is a polling thread that accesses network submission and completion queues, and make calls into the key-value engine.  This single-threaded design means that shard threads should not block - an invocation on the key-value engine should not block since other client requests will be stalled until completion.

### Operations

The C++ client interface is defined in headers *src/components/api/mcas_itf.h* and *src/components/api/kvstore_itf.h*

High-level summary of operations:

Operation | Description 
----------|-------------
```create_pool``` | Create a named memory pool 
```open_pool```   | Open an existing memory pool
```delete_pool``` | Delete an existing memory pool
```configure_pool``` | Configure pool (e.g., AddIndex::VolatileTree)
```put``` | Put based on memcpy between application and network
```put_direct``` | Put using zero-copy via registered IO memory. Typically used for large data transfers (e.g., > 1MB)  and GPU-Direct
```get``` | Get based on memcpy. API-allocated memory
```get_direct``` | Get based on zero-copy into registered IO memory. Typically used for large data transfers (e.g,. > 1MB) and GPU-Direct
```find``` | Use index to find a key via simple match, regex, etc.
```erase``` | Erase an key-value pair 
```count``` | Return number of objects in a pool
```get_attribute``` | Get attribute of pool or object (e.g., Attribute::VALUE_LEN)
```register_direct_memory``` | Register client-allocated memory for zero-copy RDMA use
```unregister_direct_memory``` | Unregister memory from RDMA subsystem
```free_memory``` | Free memory returned by a get call

