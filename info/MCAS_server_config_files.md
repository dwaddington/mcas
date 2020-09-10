# MCAS Server Configuration

The MCAS server process launch requires a configuration file parameter. For example:

```bash
./mcas --conf ./myConfig.conf --debug 0
```

Example MCA configuration file:

```json
{
    "shards" :
    [
        {
            "core" : 0,
            "port" : 11911,
            "net"  : "mlx5_0",
            "default_backend" : "hstore",
            "default_ado_path" : "/opt/mcas/build/dist/bin/ado",
            "default_ado_plugin" : "libcomponent-adoplugin-graph.so"
            "dax_config" : [{ "region_id": 0, "path": "/dev/dax0.0", "addr": "0x9000000000" }]
        }
    ],
    "net_providers" : "verbs",
    "resources":
    {
            "ado_cores":"6-8",
            "ado_manager_core": 1
    }
}
```


| Item | Subitem | Description | Example |
| --- | --- | --- | --- |
| shards | core | Core number (counting from 0) to bind to | 0 |
| | port | TCP/IP port to listen on (includes RDMA bootstrap). Should be unique for each shard. | 11911 |
| | net | Network device | "mlx5_0", "mlx5_1", "eth0" |
| | default_backend | Backend key-value engine component | "hstore", "mapstore" |
| (*ADO only*)| default\_ado\_path | Path for ADO plugin components | "/install_dir/bin/ado" |
| (*ADO only*)| default\_ado\_plugin | Name of default plugin | "libcomponent-adoplugin-graph.so" |
| (*hstore only*) | dax_config | DAX region assignment  |
| dax_config | region_id | Unique region identifier | 0 |
| | path | Device DAX path | "/dev/dax0.0", "/dev/dax1.9" |
| | addr | Virtual address space to map to | "0x900000000" |
| net_providers | - | Network provider (libfabric) | "verbs", "sockets" |
| resources | ado_cores | Cores to allocate for ADO process | "3-4" |
| | ado\_manager\_core | Core for ADO management thread | 2 |



