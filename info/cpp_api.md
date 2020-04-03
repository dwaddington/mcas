# C++ API

MCAS is implemented using a C++ component-based design.  Components are dynamic link libraries with a predefined interface (`Component::IBase`) for querying support for other interface (Microsoft COM like).  The client API for MCAS is implemented as a component (`libcomponent-mcasclient.so`) that can be dynamically linked to the application.  The interface definition, and details of the API, are given in `src/components/api/mcas_itf.h`.  The client API is thread-safe.

The following example code is available in `examples/cpp_basic`.

Open MCAS client component factory and create session instance on a specific Shard that corresponds to some TCP/IP address.

```cpp
/* load component and create factory */
IBase *comp = load_component("libcomponent-mcasclient.so", 
                             mcas_client_factory);
                             
auto factory = static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid()));
  
/* create instance of MCAS client session */
auto mcas = factory->mcas_create(1 /* debug level, 0=off */,
                                 getlogin(),
                                 Options.addr, /* MCAS server endpoint */
                                 Options.device); /* see mcas_client.h */
factory->release_ref();
                                
```

Open existing pool or create one:

```cpp
const std::string poolname = "myBasicPool";
auto pool = mcas->open_pool(poolname, 0);

if (pool == IKVStore::POOL_ERROR) {
  /* ok, try to create pool instead */
  pool = mcas->create_pool(poolname, MB(32));
}
```

Put an item in the pool:

```cpp
if(mcas->put(pool,
             key,
             value) != S_OK)
  throw General_exception("put failed unexpectedly.");
```

And retrieve it back:

```cpp
if(mcas->get(pool, key, retrieved_value) != S_OK)
  throw General_exception("get failed unexpectedly.");
```

Finally, clean up:

```cpp
mcas->close_pool(pool);
mcas->release_ref();
```

