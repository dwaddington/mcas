#!/usr/bin/env python3

# Description  : PM sorting ADO chunk generator
# Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
# Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
# License      : Apache License, Version 2.0
# Done as part of the Workshop on Search Data Structures for High-Performance Databases
# at Tel-Aviv University.

import os
import sys
from random import randint


# timestamp (in seconds) chosen uniformly in range 00:00-59:59
# TODO: decide timestamp format and adjust range accordingly
def rand_timestamp():
    return randint(0, 3600)


# datacenter_id chosen uniformly in range 1-20
def rand_datacenter_id():
    return randint(1, 20)


# operation type chosen in range 1-100 with distribution:
# 1-5    w.p. 10%
# 6-20   w.p. 2%
# 21-100 w.p. 0.25%


def rand_operation_type():
    random_num = randint(0, 399)
    if random_num < 200:
        return (random_num // 40) + 1
    elif random_num < 320:
        return ((random_num - 200) // 8) + 6
    else:
        return (random_num - 320) + 21


RECORDS_IN_CHUNK = sys.argv[1]
CHUNK_NUMBER = sys.argv[2]
RECORD_SIZE = 100
BYTEORDER = 'big'

# generate chunk number CHUNK_NUMBER, starting from record CHUNK_NUMBER*RECORDS_IN_CHUNK
sys_call = "./gensort -b" + str(int(CHUNK_NUMBER) * int(RECORDS_IN_CHUNK)) + " " + RECORDS_IN_CHUNK + " tmp_chunk"
os.system(sys_call)
chunk_file = open("tmp_chunk", 'rb')
record_list = []
for i in range(int(RECORDS_IN_CHUNK)):
    record_list.append(bytearray(chunk_file.read(RECORD_SIZE)))
datacenter_id = rand_datacenter_id().to_bytes(2, byteorder=BYTEORDER, signed=False)
for record in record_list:
    record[0:5] = rand_timestamp().to_bytes(5, byteorder=BYTEORDER, signed=False)
    record[5:7] = datacenter_id
    record[7:10] = rand_operation_type().to_bytes(3, byteorder=BYTEORDER, signed=False)
skewed_chunk_file = open("L0chunk" + CHUNK_NUMBER, 'wb')
for record in record_list:
    skewed_chunk_file.write(record)

chunk_file.close()
os.remove("tmp_chunk")
skewed_chunk_file.close()
