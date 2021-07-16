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

    log("Testing: np.ndarray RHS ...")
    s.w = np.ndarray((100,100),dtype=np.uint8)
    s.w = np.ndarray([2,2,2],dtype=np.uint8)
    w = np.ndarray([2,2,2],dtype=np.uint8)
    s.w.fill(9)
    w.fill(9)    
    if np.array_equal(s.w, w) != True:
        raise RuntimeError('ndarray equality failed')

    log("Testing: pymm.ndarray RHS ...")
    s.x = pymm.ndarray((100,100),dtype=np.uint8)
    s.x = pymm.ndarray([2,2,2],dtype=np.uint8)
    x = np.ndarray([2,2,2],dtype=np.uint8)
    s.x.fill(8)
    x.fill(8)    
    if np.array_equal(s.x, x) != True:
        raise RuntimeError('ndarray equality failed')

    s.erase('x')
    s.erase('w')

    s.r = np.ndarray((3,4,))
    s.r.fill(1)
    s.r2 = s.r.reshape((2,6,))
    s.r3 = s.r.reshape((1,-1,))

    s.erase('r')
    s.erase('r2')
    s.erase('r3')
        
    
    log("Testing: ndarray OK!")

# based on https://pytorch.org/tutorials/beginner/pytorch_with_examples.html

s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=True)

test_ndarray(s)

print(colored(255,255,255,"OK!"))
