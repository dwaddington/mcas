#!/usr/bin/python3 -m unittest
#
# basic numpy ndarray
#
import unittest
import pymm
import numpy as np
import math
import torch

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def print_error(*args):
    print(colored(255,0,0,*args))

def log(*args):
    print(colored(0,255,255,*args))


force_new=True

class TestNdarray(unittest.TestCase):
    def setUp(self):
        global force_new
        self.s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=force_new)
        force_new=False

    def tearDown(self):
        del self.s        
    
    def test_ndarray(self):
        log("Testing: np.ndarray RHS ...")
        self.s.w = np.ndarray((100,100),dtype=np.uint8)
        self.s.w = np.ndarray([2,2,2],dtype=np.uint8)
        w = np.ndarray([2,2,2],dtype=np.uint8)
        self.s.w.fill(9)
        w.fill(9)    
        self.assertTrue(np.array_equal(self.s.w, w))

        log("Testing: pymm.ndarray RHS ...")
        self.s.x = pymm.ndarray((100,100),dtype=np.uint8)
        self.s.x = pymm.ndarray([2,2,2],dtype=np.uint8)
        x = np.ndarray([2,2,2],dtype=np.uint8)
        self.s.x.fill(8)
        x.fill(8)
        self.assertTrue(np.array_equal(self.s.x, x))

        self.s.erase('x')
        self.s.erase('w')

    def test_ndarray_shape(self):
        self.s.r = np.ndarray((3,4,))
        self.s.r.fill(1)
        print(id(self.s.r))
    
        self.assertTrue(self.s.r.shape[0] == 3 and self.s.r.shape[1] == 4)

        self.s.r2 = self.s.r.reshape((2,6,))
        print(id(self.s.r2))

        self.assertTrue(self.s.r2.shape[0] == 2 and self.s.r2.shape[1] == 6)

        self.s.r3 = self.s.r.reshape((1,-1,))
        print(id(self.s.r3))

    
        self.s.r2.fill(2)
        self.s.r3.fill(3)
        print(self.s.r)
        print(self.s.r2)
        print(self.s.r3)
    
        self.s.erase('r')
        self.s.erase('r2')
        self.s.erase('r3')


if __name__ == '__main__':
    unittest.main()
