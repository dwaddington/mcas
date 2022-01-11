#!/usr/bin/env python3

# Description  : PM sorting ADO chunk generator
# Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
# Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
# License      : Apache License, Version 2.0
# Done as part of the Workshop on Search Data Structures for High-Performance Databases
# at Tel-Aviv University.

import os

for i in range(512):
    sys_call = "./chunk_generator.py 1280000 " + str(i)
    print("generating chunk " + str(i) + "...")
    os.system(sys_call)
    print("chunk " + str(i) + " generated")
