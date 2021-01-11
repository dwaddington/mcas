#!/usr/bin/python3
import argparse

TEMPLATE="""
{
    \"shards\" : 
    [
      %%SHARDS%%
    ],
    \"net_providers\" : \"verbs\"
}
"""

SHARD="""
     	{
            \"core\" : 0,
            \"port\" : %%PORT%%,
            \"net\"  : \"mlx5_0\",
            \"default_backend\" : \"hstore\",
            \"dax_config\" : [{ \"path\": \"/dev/dax0.0\", \"addr\": \"%%LOADADDR%%" }]
        }
"""

parser = argparse.ArgumentParser(description='Generate configuration file.')
parser.add_argument('--port', metavar='N', type=int, nargs='?', help='network port')
#parser.add_argument('--sum', dest='accumulate', action='store_const',
#                    const=sum, default=max,
#                    help='sum the integers (default: find the max)')

args = parser.parse_args()
print(args.port)

if args.port != None:
    SHARD=SHARD.replace("%%PORT%%", str(args.port))

TEMPLATE=TEMPLATE.replace("%%SHARDS%%", SHARD)
    
print(TEMPLATE)
