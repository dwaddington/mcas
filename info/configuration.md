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
	                        "description": "When the value iof netproviders is 'verbs', the default is 11911. When 'sockets', 11921 An IP port number",
	                        "type": "integer",
	                        "minimum": "0",
	                        "maximum": "65535",
	                        "default": "11911"
	                    },
	                    "core": {
	                        "description": "CPU core to which the shard thread should be assigned",
	                        "type": "integer",
	                        "minimum": "0"
	                    },
	                    "index": {
	                        "description": "Unused.",
	                        "type": "string"
	                    },
	                    "net": {
	                        "description": "Device on which to listen for clients (alternative to addr).",
	                        "type": "string"
	                    },
	                    "addr": {
	                        "description": "IPv4 address on which to listen for clients (alternative to net).",
	                        "type": "string"
	                    },
	                    "default_backend": {
	                        "description": "Key/value store implementation to use.",
	                        "type": "string"
	                    },
	                    "dax_config": {
	                        "description": "An array. The schema for items is up to dax_map. See its schema.",
	                        "type": "array"
	                    },
	                    "ado_plugins": {
	                        "description": "An array of ADO shared libraries to load.",
	                        "type": "array",
	                        "items": "string"
	                    },
	                    "ado_params": {
	                        "description": "An array of ADO shared libraries to load.",
	                        "type": "object",
	                        "additionalProperties": {
	                            "type": "string"
	                        },
	                        "description": "Key/value pairs. The values must be strings"
	                    },
	                    "ado_cores": {
	                        "description": "Cores to use for ADO processes. A comma-separated list of CPU core numbers, or inclusive ranges, or boths",
	                        "type": "string",
	                        "pattern": "([0-9]+([-:][0-9]+)?)(,[0-9]+([-:][0-9]+)?)*"
	                    },
	                    "ado_core_count": {
	                        "description": "A scheduling parameter for ADO. Perhaps it indicates the expected CPU load, relative to other AOO prcoesses.",
	                        "type": "number"
	                    }
	                },
	                "required": [
	                    "core"
	                ]
	            }
	        },
	        "ado_path": {
	            "description": "Full patch to the server ADO plugin manager executable.",
	            "type": "string"
	        },
	        "net_providers": {
	            "description": "ilibfabric net provider ('verbs' or 'sockets')",
	            "type": "string"
	        },
	        "resources": {
	            "description": "ADO(?) resources",
	            "type": "object",
	            "additionalProperties": false,
	            "properties": {
	                "ado_cores": {
	                    "description": "Cores which may be used for non shard-specific ADO threads? A comma-separated list of CPU core numbers, or inclusive ranges, or boths",
	                    "type": "string",
	                    "pattern": "([0-9]+([-:][0-9]+)?)(,[0-9]+([-:][0-9]+)?)*"
	                },
	                "ado_manager_core": {
	                    "description": "ADO manager core",
	                    "type": "integer",
	                    "minimum": "0"
	                }
	            }
	        },
	        "security": {
	            "description": "Security parameters",
	            "type": "object",
	            "properties": {
	                "cert": {
	                    "description": "Certificate file name?",
	                    "type": "string"
	                }
	            },
	            "required": [
	                "cert"
	            ],
	            "additionalProperties": false
	        },
	        "cluster": {
	            "description": "Some sort of cluster identificaton",
	            "type": "object",
	            "properties": {
	                "group": {
	                    "description": "cluster group?",
	                    "type": "string"
	                },
	                "name": {
	                    "description": "cluster name, or node name?",
	                    "type": "string"
	                },
	                "addr": {
	                    "description": "cluster server address?",
	                    "type": "string"
	                },
	                "port": {
	                    "description": "cluster port? An IP port number",
	                    "type": "integer",
	                    "minimum": "0",
	                    "maximum": "65535",
	                    "default": "11800"
	                }
	            },
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

Three back ends are available:
 - mapstore: a non-persistent KV store, for speed and for testing
 - hstore: a persistent KV store
 - hstore-cc: a variation fo hstore with less steady-state speed but faster crash build.

Lists of cores are comma-separated list of cores and/or ranges of cores, on which to pin ADO threads. Cores are integer strings; a range of cores is either an inclusive ranet of cores separated byu a hyphen, e.g. 4-6, or an initial core and count of cores separated by a colon, e.g 4:3.

The elements ado_core, and ado_manager_core are optional, but are recommended to be present, and to identify disjoint sets of cores, in order to improve cache locality.

DAX Configuration Syntax
===

The DAX configuration is specified by a separate component, but often needss to be placed within a server configuration, The DAX configuration schema is:

	{
	    "description": "The DAX memory soaces are made available to a shard for persistend storage.",
	    "type": "array",
	    "items": {
	        "description": "Identification of DAX imemory space to use as one 'region' of a shard's persistend storage",
	        "type": "object",
	        "properties": {
	            "region_id": {
	                "description": "An integer, unique in the array",
	                "examples": "0",
	                "type": "integer",
	                "minimum": "0"
	            },
	            "path": {
	                "description": "Full path to teh DAX file to be used",
	                "examples": "/dev/dax0.0",
	                "type": "string"
	            },
	            "addr": {
	                "description": "Virtual address to which to mmap the DAX file. An integer, but when used within a server configuration, a JSON string representing a C-style hexadecimal string is acceptedm and will be converted to an integer",
	                "examples": "241591910",
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

If the The addr parameter of a DAX configuration region embedded in a Server Configuration is specified as a string, rather than an integer, it will be converted to an integer by std::stoull. This permits easlity recognizeable specificatoins of base-2 aligned addresses such as "0x900000000".
