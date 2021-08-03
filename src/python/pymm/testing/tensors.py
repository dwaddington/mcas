#!/usr/bin/python3 -m unittest
#
# testing transient memory (needs modified Numpy)
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
class TestTensors(unittest.TestCase):
    def setUp(self):
        global force_new
        self.s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=force_new)
        force_new=False
    
    def tearDown(self):
        del self.s


    def test_torch_tensor(self):
        log("Testing: torch_tensor")
        n = torch.Tensor(np.arange(0,1000))
        self.s.x = torch.Tensor(np.arange(0,1000)) #pymm.torch_tensor(n)

        # shelf type S
        self.assertTrue(str(type(self.s.x)) == "<class 'pymm.torch_tensor.shelved_torch_tensor'>")

        log("Testing: torch_tensor sum={}".format(sum(self.s.x)))        
        self.assertTrue(self.s.x.sum() == 499500)

        slice_sum = sum(self.s.x[10:20])
        log("Testing: torch_tensor slice sum={}".format(slice_sum))
        self.assertTrue(slice_sum == 145)

        # shelf type S after in-place operation
        self.assertTrue(str(type(self.s.x)) == "<class 'pymm.torch_tensor.shelved_torch_tensor'>")
        
        # shelf type S * NS (non-shelf type)
        self.assertTrue(str(type(self.s.x * n)) == "<class 'torch.Tensor'>")
        
        # shelf type NS * S
        self.assertTrue(str(type(n * self.s.x)) == "<class 'torch.Tensor'>")

        # shelf type S * shelf type S
        self.assertTrue(str(type(self.s.x * self.s.x)) == "<class 'torch.Tensor'>")
    
        self.s.x += 1
        self.s.x *= 2
        self.s.x -= 0.4
        self.s.x /= 2

        self.s.erase('x')
        log("Testing: torch_tensor OK!")


if __name__ == '__main__':
    unittest.main()
