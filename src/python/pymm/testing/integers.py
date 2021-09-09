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

    def test_integer_assignment(self):
        log("Test: integer assignment and re-assignment")
        self.s.a = 2
        self.s.b = 30
        self.s.b = self.s.b / self.s.a
        print(self.s.b)
        self.s.erase('a')
        self.s.erase('b')

    def test_integer_with_ndarray(self):
        log("Test: array change with integer")
        self.s.count = 1
        self.s.count += 1
        self.s.n = np.arange(0,9).reshape(3,3)
        print(self.s.n)
        self.s.p = self.s.n / 3
        print(self.s.p)
        self.s.p = self.s.n / int(self.s.count)
        print(self.s.p)
        self.s.p = self.s.n / self.s.count
        print(self.s.p)
        self.s.erase('p')
        self.s.erase('n')

    def test_rmod_operation(self):
        log("Test: rmod op")
        self.s.c = 10
        print('18%10={}'.format(18 % self.s.c))
        print('10%3={}'.format(self.s.c % 3))
        self.assertTrue(18 % self.s.c == 8)
        self.assertTrue(self.s.c % 3 == 1)

        
        
        
        

if __name__ == '__main__':
    unittest.main()

