#!/usr/bin/python3 -m unittest
#
# testing transient memory (needs modified Numpy)
#
import unittest
import pymm
import numpy as np
import torch

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def log(*args):
    print(colored(0,255,255,*args))

force_new=True
class TestTensors(unittest.TestCase):
    def setUp(self):
        global force_new
        self.s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=force_new)
        force_new=False
    
    def tearDown(self):
        del self.s

    def test_bytes(self):
        log("Testing: pymm.bytes shadow type")
        self.s.x = pymm.bytes('Hello world','utf-8')
        print(self.s.x)



if __name__ == '__main__':
    unittest.main()
