#!/usr/bin/python3

import glob
import os

# Locations for devdax stores are: /dev/dax<n>.*

def candidates(pfx: str):
    return sorted([f for f in glob.glob("{}.*".format(pfx)) if os.access(f, os.R_OK) and os.access(f, os.W_OK)])

if __name__ == '__main__':
    import argparse
    import re
    parser = argparse.ArgumentParser(description='Develop a list of candidate devdax devices')
    parser.add_argument("--prefix", nargs='?', default="/dev/dax0", help="DEVDAX prefix, e,g. /dev/dax0")

    args = parser.parse_args()

    print(' '.join(candidates(args.prefix)))
