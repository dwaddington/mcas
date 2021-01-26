#!/usr/bin/python3
import argparse
import re

###############################################################
# gen-config.py tool for generating MCAS configuration files
###############################################################

TEMPLATE="""{
    \"shards\" :
    [
      %%SHARDS%%
    ],
    \"net_providers\" : \"verbs\"
}
"""

SHARD="""
        {
            \"core\" : %%CORE%%,
            \"port\" : %%PORT%%,
            \"net\"  : \"%%NETDEV%%\",
            \"default_backend\" : \"%%BACKEND%%\", %%ADO%%
            \"dax_config\" : [{ \"path\": \"%%PATH%%\", \"addr\": \"%%LOADADDR%%" }]
        }"""


def auto_int(x):
    return int(x, 0)

def replace(config, var, label):
    if var != None:
        config=config.replace(label, str(var))

def increment_dax_path(path):
    result = re.match('/dev/dax(\d+).(\d+)', path)
    if result != None:
        region=result.group(1)
        device=int(result.group(2))
        return '/dev/dax' + region + '.' + str(device + 1)
    return path

def build_shard_section(shard_count):
    global SHARD
    port = args.port
    loadaddr = args.loadbase
    path = args.path
    core = args.core
    adocores  = args.adocores
    result = ""
    ado = ""
    if args.ado != None:
        ado = '\n            "ado_plugins" : ["' + args.ado + '"], "ado_cores" : "%%ADOCORES%%", '

    while shard_count > 0:
        shard_count -= 1
        new_shard = SHARD
        new_shard = new_shard.replace("%%NETDEV%%", str(args.net))
        new_shard = new_shard.replace("%%PORT%%", str(port))
        new_shard = new_shard.replace("%%LOADADDR%%", hex(loadaddr))
        new_shard = new_shard.replace("%%PATH%%", path)
        new_shard = new_shard.replace("%%BACKEND%%", args.backend)
        new_shard = new_shard.replace("%%CORE%%", str(core))
        new_shard = new_shard.replace("%%ADO%%", ado)
        if args.ado != None:
            if args.adocores != None:
                new_shard = new_shard.replace("%%ADOCORES%%", args.adocores)
            else:
                core += 1
                new_shard = new_shard.replace("%%ADOCORES%%", str(core))
        result = result + new_shard
        if shard_count > 0:
            result += ','
        # increment fields
        path = increment_dax_path(path)
        port += 1
        core += 1
        loadaddr += 0x100000000

    return result

parser = argparse.ArgumentParser(description='Generate configuration file.')
parser.add_argument('--port', metavar='N', type=int, nargs='?', help='network port', default=11911)
parser.add_argument('--loadbase', type=auto_int, help='base address for loading (e.g. 0x900000000)', default=0x900000000)
parser.add_argument('--net', default='mlx5_0', help='network device (e.g., mlx5_0)')
parser.add_argument('--path', default='/dev/dax0.0', help='persistent memory path (e.g., /dev/dax0.0)')
parser.add_argument('--shards', type=int, default=1, help='number of shards (default=1)')
parser.add_argument('--core', type=int, default=0, help='starting core (default=0)')
parser.add_argument('--ado', help='optional ado plugin name (e.g., libcomponent-adoplugin-testing.so)')
parser.add_argument('--adocores', help='optional ADO cores (e.g. 12-23)')
parser.add_argument('--backend', default='hstore', help='backend storage engine (e.g., hstore, mapstore)')

args = parser.parse_args()

# build shard section
shard_section=build_shard_section(args.shards)

# build final configuration file
TEMPLATE=TEMPLATE.replace("%%SHARDS%%", shard_section)

print(TEMPLATE)
