#!/usr/bin/python3 -m unittest
#
# basic numpy ndarray
#
import unittest
import pymm
import numpy as np
import math

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def print_error(*args):
    print(colored(255,0,0,*args))

def log(*args):
    print(colored(0,255,255,*args))

shelf = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=True)

class TestNdarray(unittest.TestCase):
    
    def test_dlpack_array(self):
        log("Testing: dlpack_array ...")
        shelf.a = pymm.dlpack_array((5,2,4,),dtype=np.float64)
        print(shelf.a)

if __name__ == '__main__':
    unittest.main()

    
