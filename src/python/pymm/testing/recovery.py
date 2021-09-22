#!/usr/bin/python3 -m unittest
#
# testing recovery
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

def fail(*args):
    print(colored(255,0,0,*args))

def check(expr, msg):
    if expr == True:
        msg = "Check: " + msg + " OK"
        log(msg)
    else:
        msg = "Check: " + msg + " FAILED"
        fail(msg)


class TestRecovery(unittest.TestCase):

    def test_establish(self):
        log("Testing: establishing shelf and values")
        shelf = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=True)

        # different types
        shelf.s = "Hello"
        shelf.f = 1.123
        shelf.fm = 2.2
        shelf.fm += 1.1
        shelf.i = 911
        shelf.nd = np.ones((3,))
        shelf.nd2 = np.identity(10)
        shelf.b = 'This is a bytes type'
        shelf.bm = 'This is a '
        shelf.bm += 'modified bytes type'
        
        del shelf
        gc.collect()

    def test_recovery(self):
        log("Testing: recovering shelf and values")
        shelf = pymm.shelf('myShelf',pmem_path='/mnt/pmem0',force_new=False)

        print(shelf.f)
        print(shelf.fm)
        print(shelf.i)
        print(shelf.nd)
        print(shelf.nd2)
        print(shelf.b)
        print(shelf.bm)
               
        check(shelf.s == 'Hello', 'string recovery')
        check(shelf.f == 1.123, 'float recovery')
        check(round(shelf.fm,2) == 3.30, 'float modified recovery')
        check(shelf.i == 911, 'integer recovery')
        check(np.array_equal(shelf.nd, np.ones((3,))),'1D ndarray')
        check(np.array_equal(shelf.nd2, np.identity(10)),'2D ndarray')
        check(str(shelf.b) == 'This is a bytes type', 'bytes')
        
if __name__ == '__main__':
    unittest.main()
