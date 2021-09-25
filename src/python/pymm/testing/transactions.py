#!/usr/bin/python3 -m unittest
#
# basic transactions
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

shelf = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=True)
# shelf = pymm.shelf('myShelf',pmem_path='/mnt/pmem0')

class TestTransactions(unittest.TestCase):

    def test_transactions(self):
        log("Testing: transaction on matrix fill ...")
        shelf.s = 'This is a string!'
        shelf.f = 1.123
        shelf.i = 645338
        shelf.b = b'Hello'
        shelf.n = pymm.ndarray((100,100),dtype=np.uint8)
        shelf.w = np.ndarray((100,100),dtype=np.uint8)
        shelf.w.fill(ord('a'))
        shelf.l = pymm.linked_list()
        shelf.l.append(1)
        shelf.l.append(2)
        shelf.t = pymm.torch_tensor(np.arange(0,10))
        
        print(shelf.items)
        shelf.inspect(verbose=False)
        shelf.persist()
        shelf.inspect(verbose=False)
        
if __name__ == '__main__':
    unittest.main()
