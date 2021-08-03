#!/usr/bin/python3 -m unittest
#
# testing for different backends
#
import unittest
import pymm
import numpy as np
import math
import torch
import os

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def print_error(*args):
    print(colored(255,0,0,*args))

def log(*args):
    print(colored(0,255,255,*args))

class TestBackends(unittest.TestCase):
    def test_default(self):
        log("Running shelf with default backend ...")
        s = pymm.shelf('myShelf',size_mb=8,pmem_path='/mnt/pmem0',force_new=True)
        s.items
        log("OK!")

    def test_dram_mapstore(self):
        log("Running shelf with mapstore and default MM plugin ...")
        s = pymm.shelf('myShelf2',size_mb=8,backend='mapstore') # note, no force_new or pmem_path
        s.x = pymm.ndarray((10,10,))
        print(s.items)
        log("OK!")

    def test_dram_mapstore_jemalloc(self):
        log("Running shelf with mapstore and jemalloc MM plugin ...")
        s = pymm.shelf('myShelf3',size_mb=8,backend='mapstore',mm_plugin='libmm-plugin-jemalloc.so')
        s.x = pymm.ndarray((10,10,))
        s.y = pymm.ndarray((100,100,))
        print(s.items)
        log("OK!")

    def test_dram_mapstore_rcalb(self):
        log("Running shelf with mapstore and rcalb MM plugin ...")
        s = pymm.shelf('myShelf4',size_mb=8,backend='mapstore',mm_plugin='libmm-plugin-rcalb.so')
        s.x = pymm.ndarray((10,10,))
        s.y = pymm.ndarray((100,100,))
        print(s.items)
        log("OK!")
    
    def test_hstore_cc(self):
        log("Running shelf with hstore-cc ...")
        os.system("rm -Rf /mnt/pmem0/test_hstore_cc")
        os.mkdir("/mnt/pmem0/test_hstore_cc")
        s = pymm.shelf('myShelf5',size_mb=8,backend='hstore-cc',pmem_path="/mnt/pmem0/test_hstore_cc")
        s.x = pymm.ndarray((10,10,))
        s.y = pymm.ndarray((100,100,))
        print(s.items)
        log("OK!")

    def test_hstore_mm_rcalb(self):
        log("Running shelf with hstore-mm and rcalb MM plugin ...")
        os.system("rm -Rf /mnt/pmem0/test_hstore_mm_rcalb")
        os.mkdir("/mnt/pmem0/test_hstore_mm_rcalb")
        s = pymm.shelf('myShelf6',size_mb=8,backend='hstore-mm',pmem_path="/mnt/pmem0/test_hstore_mm_rcalb",mm_plugin='libmm-plugin-rcalb.so')
        s.x = pymm.ndarray((10,10,))
        s.y = pymm.ndarray((100,100,))
        print(s.items)
        log("OK!")

    def test_hstore_mm_jemalloc(self):
        log("Running shelf with hstore-mm and jemalloc MM plugin ...")
        os.system("rm -Rf /mnt/pmem0/test_hstore_mm_jemalloc")
        os.mkdir("/mnt/pmem0/test_hstore_mm_jemalloc")
        s = pymm.shelf('myShelf7',size_mb=8,backend='hstore-mm',pmem_path="/mnt/pmem0/test_hstore_mm_jemalloc",mm_plugin='libmm-plugin-jemalloc.so')
        s.x = pymm.ndarray((10,10,))
        s.y = pymm.ndarray((100,100,))
        print(s.items)
        log("OK!")
        


    
if __name__ == '__main__':
    unittest.main()
