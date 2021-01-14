# ADO Signals (Experimental Feature)

MCAS can be configured to allow ADOs to receive notification about put
(or put_direct), get (or get_direct) and erase events (outside of the
`invoke_ado`, `invoke_put_ado`).

Signals are configured on a per-shard basis, such as follows:

```json
{
    "shards" : 
    [
      
     	{
            "core" : 0,
            "port" : 11910,
            "net"  : "mlx5_0",
            "default_backend" : "hstore", 
            "dax_config" : [{ "path": "/dev/dax0.0", "addr": "0x900000000" }],
            "ado_plugins" : ["libcomponent-adoplugin-testing.so"],
            "ado_cores", "2"
            "ado_signals" : ["post-put", "post-get", "post-erase"]
        }
    ],
    "ado_path" : "/home/mcas/dist/bin/ado",
    "net_providers" : "verbs"
}
```

When ADO signals are configured, the shard thread invokes
`Shard::signal_ado` to upcall the ADO process via UIPC.  During the
upcall, the corresponding key is read-locked.  The upcall is
dispatched as a work request with a message prefixed with
`ADO::Signal::`.  Therefore, ultimately this is received by the
`do_work` function in the ADO plugin.

The original base invocation does not return a result to the client
until the upcall to the ADO is complete.  Responses from the ADO are
not propogated to the client.

