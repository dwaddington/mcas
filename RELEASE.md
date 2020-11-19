# Release v0.5.0 (major release)

## Key new features

* Support for fsdax PMEM configuration (info/MCAS_notes_on_dax.md).
* Use of Mellanox/NVIDIA Connect-X ODP (On-demand Paging) via USE_ODP=1
* Experimental wrapper for Rust-basd ADO implementation (src/ado/rust-wrapper)

## Limitations

* Rust-based ADO expects UTF8 interpretable keys.

# Release v0.4.0 (major release)

## Key new features

* Support for Active Data Objects (ADO) - requires kernel module
* Python/C++ client API
* Container and VM based deployment
* Configurable with volatile reconstituting memory allocator (hstore) or persistent allocator (hstore-cc)

## Limitations

* DRAM (mapstore) and devdax based PMEM (hstore, hstore-cc)
