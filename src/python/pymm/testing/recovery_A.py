#!/usr/bin/python3 -m unittest
#
# "before" part of recovery test 
#
import unittest
import pymm
import numpy as np
import math
import os

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def log(*args):
    print(colored(0,255,255,*args))
    
class TestBefore(unittest.TestCase):
    def setUp(self):
        global force_new
        os.system("rm -Rf /mnt/pmem0/1")
        os.system("rm -Rf /mnt/pmem0/2")
        os.system("mkdir -p /mnt/pmem0/1")
        os.system("mkdir -p /mnt/pmem0/2")
        self.s = pymm.shelf('myShelf', size_mb=1024, backend='hstore-cc', pmem_path='/mnt/pmem0/1', load_addr=0x900000000, force_new=True)

        # can't create two shelves of the same type (we need to do some work around setting the base address)
        #self.s2 = pymm.shelf('myShelf-2',size_mb=1024, backend='hstore-cc', pmem_path='/mnt/pmem0/2',force_new=True)
        print("Setup OK")
        
    def tearDown(self):
        del self.s
        #del self.s2
        
    def test_write_A(self):
        self.s.A = np.ndarray((3,8),dtype=np.uint8)
        self.s.A.fill(1)
        self.s.A[1] += 1
        print(self.s.A)

    def xtest_write_B(self):
        self.s.B = np.zeros((3,8,))
        print(self.s.B)

    def xtest_write_C(self):
        self.s2.C = 99
        
        

if __name__ == '__main__':
    unittest.main()
