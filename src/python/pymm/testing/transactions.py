#!/usr/bin/python3 -m unittest
#
# basic shelf assignment tests
#
import unittest
import pymm
import numpy as np
import math
import torch

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def log(*args):
    print(colored(0,255,255,*args))
    
force_new=True
class TestTransactions(unittest.TestCase):
    def setUp(self):
        global force_new
        self.s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=force_new)
        force_new=False
    
    def tearDown(self):
        del self.s
        
    def test_transactions(self):
        log("Testing: transaction on matrix fill ...")
        self.s.w = np.ndarray((100,100),dtype=np.uint8)
        self.s.w.fill(9)
        print(self.s.items)
        
if __name__ == '__main__':
    unittest.main()
