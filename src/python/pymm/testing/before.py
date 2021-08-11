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
    
force_new=True
class TestBefore(unittest.TestCase):
    def setUp(self):
        global force_new
        os.system("mkdir -p /mnt/pmem0/a")
        self.s = pymm.shelf('myShelf-A',size_mb=1024,pmem_path='/mnt/pmem0/a',force_new=force_new)
        force_new=False
    
    def tearDown(self):
        del self.s
        
    def test_write_A(self):
        self.s.A = np.ndarray((3,8),dtype=np.uint8)
        self.s.A.fill(1)
        self.s.A[1] += 1
        print(self.s.A)

    def test_write_B(self):
        self.s.B = np.zeros((3,8,))
        print(self.s.B)
        

if __name__ == '__main__':
    unittest.main()
