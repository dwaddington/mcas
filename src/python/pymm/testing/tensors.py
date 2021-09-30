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

    def test_torch_tensor_shadow_list(self):
        log("Testing: tensor ctor")
        self.s.x = pymm.torch_tensor([1,1,1,1,1])
        print(self.s.x)

    def test_torch_tensor_shadow_ndarray_A(self):
        log("Testing: tensor ctor")
        self.s.y = pymm.torch_tensor(np.arange(0,10))
        self.s.y.fill(-1.2)
        print(self.s.y)

    def test_torch_tensor_shadow_ndarray_B(self):
        log("Testing: tensor ctor")
        print(self.s.y)
        self.s.y = pymm.torch_tensor(np.arange(0,10))
        self.s.y.fill(-1.3)
        print(self.s.y)

    def test_torch_tensor_copy(self):
        log("Testing: tensor copy")
        T = torch.tensor([[1,1,1,1,1],[2,2,2,2,2],[3,3,3,3,3]])
        U = torch.tensor([[1,1,1,1,1],[2,2,2,2,2],[3,3,3,3,3]])
        print(T.shape)
        self.s.x = T
        print(self.s.x)
        self.assertTrue(self.s.x.equal(T))
        Q = torch.tensor([[1,2,3],[4,5,6]])

    def test_torch_ones(self):
        log("Testing: torch ones")
        self.s.x = torch.ones([3,5],dtype=torch.float64)
        print(self.s.x)
        self.s.x += 0.5
        print(self.s.x)

    def test_torch_leaf(self):
        log("Testing: torch tensor leaf")
        self.s.x = torch.randn(1, 1)
        self.assertTrue(self.s.x.is_leaf)

    def test_torch_zerodim_shadow(self):
        log("Testing: zero dim shadow")
        self.s.x = pymm.torch_tensor(1.0)
        self.assertTrue(self.s.x.dim() == 0)
        print(type(self.s.x))
        self.assertTrue(str(type(self.s.x)) == "<class 'pymm.torch_tensor.shelved_torch_tensor'>")

    def test_torch_zerodim(self):
        log("Testing: zero dim copy")
        self.s.y = torch.tensor(2.0)
        self.assertTrue(self.s.y.dim() == 0)
        self.assertTrue(str(type(self.s.y)) == "<class 'pymm.torch_tensor.shelved_torch_tensor'>")

    # NOT SUPPORTED
    def NORUN_test_torch_require_grad(self):
        log("Testing: requires_grad= param")
        self.s.x = torch.tensor(1.0, requires_grad = True)
        self.s.z = self.s.x ** 3
        self.s.z.backward() #Computes the gradient 
        print(self.s.x.grad.data) #Prints '3' which is dz/dx 
        
    def test_torch_tensor(self):
        log("Testing: torch_tensor")
        n = torch.Tensor(np.arange(0,1000))
        self.s.t = torch.Tensor(np.arange(0,1000)) #pymm.torch_tensor(n)

        # shelf type S
        self.assertTrue(str(type(self.s.t)) == "<class 'pymm.torch_tensor.shelved_torch_tensor'>")

        log("Testing: torch_tensor sum={}".format(sum(self.s.t)))        
        self.assertTrue(self.s.t.sum() == 499500)

        slice_sum = sum(self.s.t[10:20])
        log("Testing: torch_tensor slice sum={}".format(slice_sum))
        self.assertTrue(slice_sum == 145)

        # shelf type S after in-place operation
        self.assertTrue(str(type(self.s.t)) == "<class 'pymm.torch_tensor.shelved_torch_tensor'>")
        
        # shelf type S * NS (non-shelf type)
        self.assertTrue(str(type(self.s.t * n)) == "<class 'torch.Tensor'>")
        
        # shelf type NS * S
        self.assertTrue(str(type(n * self.s.t)) == "<class 'torch.Tensor'>")

        # shelf type S * shelf type S
        self.assertTrue(str(type(self.s.t * self.s.t)) == "<class 'torch.Tensor'>")
    
        self.s.t += 1
        self.s.t *= 2
        self.s.t -= 0.4
        self.s.t /= 2

        self.s.erase('t')
        log("Testing: torch_tensor OK!")


if __name__ == '__main__':
    unittest.main()
