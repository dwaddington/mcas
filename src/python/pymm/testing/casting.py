import pymm
import numpy as np
import math
import torch

from inspect import currentframe, getframeinfo
line = lambda : currentframe().f_back.f_lineno

def fail(msg):
    print(colored(255,0,0,msg))
    raise RuntimeError(msg)

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def print_error(*args):
    print(colored(255,0,0,*args))

def log(*args):
    print(colored(0,255,255,*args))
    

def test_ndarray(s):
    
    log("Testing: ndarray")
    s.x = pymm.ndarray((100,100))
    n = np.ndarray((100,100))

    # shelf type S
    if str(type(s.x)) != "<class 'pymm.ndarray.shelved_ndarray'>":
        fail('type check failed')
    
    s.x.fill(1)
    # shelf type S after in-place operation
    if str(type(s.x)) != "<class 'pymm.ndarray.shelved_ndarray'>":
        fail('type check failed')
    
    # shelf type S * NS (non-shelf type)
    if str(type(s.x * n)) != "<class 'numpy.ndarray'>":
        fail('type check failed')

    # shelf type NS * S
#    if str(type(n * s.x)) != "<class 'numpy.ndarray'>":
#        fail('type check failed')
    
    # shelf type S * shelf type S
    if str(type(s.x * s.x)) != "<class 'numpy.ndarray'>":
        fail('type check failed')

    s.erase('x')
    log("Testing: ndarray OK!")


def test_torch_tensor(s):
    
    log("Testing: torch_tensor")
    s.x = pymm.torch_tensor((100,100))
    n = torch.Tensor(np.ndarray((100,100)))

    # shelf type S
    if str(type(s.x)) != "<class 'pymm.torch_tensor.shelved_torch_tensor'>":
        fail('type check failed')
    
    s.x.fill(1)
    # shelf type S after in-place operation
    if str(type(s.x)) != "<class 'pymm.torch_tensor.shelved_torch_tensor'>":
        fail('type check failed')

    
    # shelf type S * NS (non-shelf type)
    if str(type(s.x * n)) != "<class 'torch.Tensor'>":
        fail('type check failed')

    # shelf type NS * S
    if str(type(n * s.x)) != "<class 'torch.Tensor'>":
        fail('type check failed')

    # shelf type S * shelf type S
    if str(type(s.x * s.x)) != "<class 'torch.Tensor'>":
        fail('type check failed')

    s.x += 1
    s.x *= 2
    s.x -= 0.4
    s.x /= 2
    
    s.erase('x')
    log("Testing: torch_tensor OK!")


# based on https://pytorch.org/tutorials/beginner/pytorch_with_examples.html

s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=True)
test_ndarray(s)
test_torch_tensor(s)
