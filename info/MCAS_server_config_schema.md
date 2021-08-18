# MCAS Config File Guide

Server Configuration Syntax
===

The JSON schema for a configuration is:

	{
	    "type": "object",
	    "additionalProperties": false,
	    "properties": {
	        "debug_level": {
	            "description": "Amount of debugging information to be emitted. Higher numbers may produce more debug messages",
	            "examples": [
	                "0",
	                "1"
	            ],
	            "type": "integer"
	        },
	        "shards": {
	            "description": "Zero or more 'shards', addressable by client through their IP/port addresses",
	            "type": "array",
	            "items": {
	                "type": "object",
	                "additionalProperties": false,
	                "properties": {
	                    "port": {
	                        "description": "When the value of netproviders is 'verbs', the default is 11911. When 'sockets', 11921 An IP port number",
	                        "examples": [
	                            "11911",
	                            "19000"
	                        ],
	                        "type": "integer",
	                        "minimum": "0",
	                        "maximum": "65535",
	                        "default": "11911"
	                    },
	                    "core": {
	                        "description": "CPU core to which the shard thread should be assigned",
	                        "examples": [
	                            "0",
	                            "5"
	                        ],
	                        "type": "integer",
	                        "minimum": "0"
	                    },
	                    "index": {
	                        "description": "Unused.",
	                        "type": "string"
	                    },
	                    "net": {
	                        "description": "Device on which to listen for clients (alternative to addr).",
	                        "examples": [
	                            "mlx5_0",
	                            "mlx4_1"
	                        ],
	                        "type": "string"
	                    },
	                    "addr": {
	                        "description": "IPv4 address on which to listen for clients (alternative to net).",
	                        "examples": [
	                            "10.0.0.1",
	                            "14.4.0.25"
	                        ],
	                        "type": "string"
	                    },
	                    "security_mode": {
	                        "description": "Security mode",
	                        "examples": [
	                            "none",
	                            "tls-hmac"
	                        ],
	                        "type": "string"
	                    },
	                    "security_port": {
	                        "description": "Port for security channel",
	                        "examples": [
	                            "0",
	                            "11922"
	                        ],
	                        "type": "integer",
	                        "minimum": "0"
	                    },
	                    "default_backend": {
	                        "description": "Key/value store implementation to use.",
	                        "examples": [
	                            "hstore",
	                            "hstore-cc",
	                            "hstore-mm",
	                            "mapstore"
	                        ],
	                        "type": "string",
	                        "type": "string"
	                    },
	                    "mm_plugin_path": {
	                        "description": "Path (absolute, as of now) to use for Key/value store memory allocator plugin. Required for hstore-mm, optional for others.",
	                        "examples": [
	                            "/opt/mcas/lib/libmm-plugin-rcalb.so",
	                            "/opt/mcas/lib/libmm-plugin-ccpm.so"
	                        ],
	                        "type": "string",
	                        "type": "string"
	                    },
	                    "dax_config": {
	                        "description": "An array. The schema for items is up to dax_map. See the DAX Configuration Schema.",
	                        "type": "array"
	                    },
	                    "ado_plugins": {
	                        "description": "An array of ADO shared libraries to load.",
	                        "examples": [
	                            "libcomponent-adoplugin-testing.so"
	                        ],
	                        "type": "array",
	                        "items": "string"
	                    },
	                    "ado_signals": {
	                        "description": "Set of ADO signals.",
	                        "examples": [
	                            "post-put",
	                            "post-get",
	                            "post-erase"
	                        ],
	                        "type": "array",
	                        "items": "string"
	                    },
	                    "ado_params": {
	                        "description": "Key/value pairs passed to ADO. The values must be strings",
	                        "examples": [
	                            "Country",
	                            "Italy",
	                            "City",
	                            "Turin"
	                        ],
	                        "type": "object",
	                        "additionalProperties": {
	                            "type": "string"
	                        },
	                        "description": "Key/value pairs. The values must be strings"
	                    },
	                    "ado_cores": {
	                        "description": "Cores to use for ADO processes. A comma-separated list of CPU core numbers or ranges, or both",
	                        "examples": [
	                            "0:3,5,7-10",
	                            "0-2,5,7:4"
	                        ],
	                        "type": "string",
	                        "pattern": "([0-9]+([-:][0-9]+)?)(,[0-9]+([-:][0-9]+)?)*"
	                    },
	                    "ado_core_count": {
	                        "description": "A scheduling parameter for ADO. Perhaps it indicates the expected CPU load, relative to other AOO prcoesses.",
	                        "examples": [
	                            "0.2",
	                            "1.4"
	                        ],
	                        "type": "number"
	                    }
	                },
	                "required": [
	                    "core"
	                ]
	            }
	        },
	        "ado_path": {
	            "description": "Full path to the ADO plugin executable.",
	            "examples": [
	                "/opt/mcas/bin/ado"
	            ],
	            "type": "string"
	        },
	        "net_providers": {
	            "description": "libfabric net provider ('verbs' or 'sockets')",
	            "examples": [
	                "verbs",
	                "sockets"
	            ],
	            "type": "string"
	        },
	        "resources": {
	            "description": "ADO resources",
	            "type": "object",
	            "additionalProperties": false,
	            "properties": {
	                "ado_cores": {
	                    "description": "Cores which may be used for non shard-specific ADO threads? A comma-separated list of CPU core numbers or ranges, or both",
	                    "examples": [
	                        "0:3,5,7-10",
	                        "0-2,5,7:4"
	                    ],
	                    "type": "string",
	                    "pattern": "([0-9]+([-:][0-9]+)?)(,[0-9]+([-:][0-9]+)?)*"
	                },
	                "ado_manager_core": {
	                    "description": "ADO manager core",
	                    "examples": [
	                        "0",
	                        "3"
	                    ],
	                    "type": "integer",
	                    "minimum": "0"
	                }
	            }
	        },
	        "security": {
	            "description": "Security parameters",
	            "type": "object",
	            "properties": {
	                "cert_path": {
	                    "description": "Default certificate file path",
	                    "examples": [
	                        "~/mcas/certs/mcas-cert.pem"
	                    ],
	                    "type": "string"
	                },
	                "key_path": {
	                    "description": "Default key file path",
	                    "examples": [
	                        "~/mcas/certs/mcas-privkey.pem"
	                    ],
	                    "type": "string"
	                }
	            },
	            "required": [
	                "cert_path"
	            ],
	            "additionalProperties": false
	        },
	        "cluster": {
	            "description": "Clustering parameters",
	            "type": "object",
	            "properties": {
	                "group": {
	                    "description": "Zyre cluster group to which the server will belong",
	                    "type": "string"
	                },
	                "name": {
	                    "description": "local node name in Zyre cluster",
	                    "type": "string"
	                },
	                "addr": {
	                    "description": "local IPv4 address for Zyre cluster communication",
	                    "type": "string"
	                },
	                "port": {
	                    "description": "local port for Zyre cluster communication An IP port number",
	                    "examples": [
	                        "11911",
	                        "19000"
	                    ],
	                    "type": "integer",
	                    "minimum": "0",
	                    "maximum": "65535",
	                    "default": "11800"
	                }
	            },
	            "required": [
	                "group",
	                "addr"
	            ],
	            "additionalProperties": false
	        }
	    },
	    "required": [
	        "shards"
	    ]
	}

Notes:
---

Shards describes the the resource which the server will use to provides shards to clients.

The server provides shard connections on the specified ports.

Alothough not checked in the schema, addr/port combinations must be unique.

Four normal back ends are available:
 - mapstore: a non-persistent KV store, for speed and for testing
 - hstore: a persistent KV store
 - hstore-cc: a variation of hstore with less steady-state speed but faster crash rebuild.
 - hstore-mm: a variation of hstore which dynamically loads its memory allocatoor from mm-plugin-path
 - hstore-mt: a variation of hstore like mm, but with some locking enabled to allow multithreaded calls to the IKVStore lock() and unlock() functions
These testing backends are also built and installed:
 - hstore-nt: like hstore, but does not write timestamps on put operations does not functions (such as some scans) which need timestamps
 - hstore-cc-pe: like hstore-cc, but supports tests which periodically cause hstore to "crash" in order to test crash consistency
 - hstore-mm-pe: like hstore-mm, but supports tests which periodically cause hstore to "crash" in order to test crash consistency

Lists of cores are comma-separated list of cores and/or ranges of cores, on which to pin ADO threads. Cores are integer strings; a range of cores is either an inclusive range of cores separated by a hyphen, e.g. 4-6, or an initial core and count of cores separated by a colon, e.g 4:3.

The elements ado_core, and ado_manager_core are optional, but are recommended to be present, and to identify disjoint sets of cores, in order to improve cache locality.

DAX Configuration Syntax
===

The DAX configuration is specified by a separate component, but often needss to be placed within a server configuration, The DAX configuration schema is:

	{
	    "description": "The DAX memory spaces are made available to a shard for persistent storage.",
	    "type": "array",
	    "items": {
	        "description": "Identification of DAX memory space to use as one 'region' of a shard's persistent storage",
	        "type": "object",
	        "properties": {
	            "region_id": {
	                "description": "An integer, unique in the array",
	                "examples": "0",
	                "type": "integer",
	                "minimum": "0"
	            },
	            "path": {
	                "description": "Full path to the DAX file to be used",
	                "examples": "/dev/dax0.0",
	                "type": "string"
	            },
	            "addr": {
	                "description": "Virtual address to which to mmap the DAX file. When used in a server configuration, a string representing a C-style hexadecimal value is accepted and converted to an integer",
	                "examples": [
	                    "241591910"
	                ],
	                "type": "integer",
	                "minimum": "0"
	            }
	        },
	        "required": [
	            "region_id",
	            "path",
	            "addr"
	        ]
	    }
	}

Notes:
---

If the The addr parameter of a DAX configuration region embedded in a Server Configuration is specified as a string, rather than an integer, it will be converted to an integer by std::stoull. This permits easlity recognizeable specifications of base-2 aligned addresses such as "0x900000000".
