#!/usr/bin/python3 -m unittest
#
# testing transient memory (needs modified Numpy)
#
import unittest
import pymm
import numpy as np
import torch
import gc

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def log(*args):
    print(colored(0,255,255,*args))

force_new=True
class TestNew(unittest.TestCase):
    def XsetUp(self):
        global force_new
        self.s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=force_new)
        force_new=False
    
    def XtearDown(self):
        del self.s

    def Xtest_bytes(self):
        log("Testing: pymm.bytes shadow type")
        self.s.x = pymm.bytes('hello world','utf-8')
        print(self.s.x)
        self.s.x += b'!  '
        print(self.s.x)
        log("Testing: pymm.bytes methods")
        print(self.s.x.capitalize())
        print(self.s.x.hex())
        print(self.s.x.strip())
        self.s.y = self.s.x.strip()
        print(self.s.y.decode())
        self.assertTrue(self.s.y.decode() == 'hello world!')
        gc.collect()

    def test_bytes_recovery_A(self):
        log("Testing: pymm.bytes (pre)recovery")
        shelf = pymm.shelf('myShelfRec',size_mb=128,pmem_path='/mnt/pmem0/reco',force_new=True)
        shelf.x = pymm.bytes('hello world','utf-8')
        shelf.s = "This is a string"
        del shelf

    def test_bytes_recovery_B(self):
        log("Testing: pymm.bytes recovery")
        shelf = pymm.shelf('myShelfRec',pmem_path='/mnt/pmem0/reco',force_new=False)
        print(shelf.s)
        print(shelf.x)




if __name__ == '__main__':
    unittest.main()
