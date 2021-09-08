#!/usr/bin/python3 -m unittest
#
# "after" part of recovery test 
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
        self.s = pymm.shelf('myShelf',size_mb=1024, backend='hstore-cc', pmem_path='/mnt/pmem0/1',force_new=False)
        #self.s2 = pymm.shelf('myShelf-2',size_mb=1024, backend='hstore-cc', pmem_path='/mnt/pmem0/2',force_new=False)
        print(self.s.items)
    
    def tearDown(self):
        del self.s
        #del self.s2        
        
    def test_check_A(self):
        print(self.s.A)
        x = np.ndarray((3,8),dtype=np.uint8)
        x.fill(1)
        x[1] += 1
        self.assertTrue(np.array_equal(self.s.A,x))

    def xtest_check_B(self):
        print(self.s.B)
        self.assertTrue(np.array_equal(self.s.B, np.zeros((3,8,))))


if __name__ == '__main__':
    unittest.main()
