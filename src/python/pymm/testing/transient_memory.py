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
    

pymm.pymmcore.enable_transient_memory()

s = pymm.shelf('myShelf',size_mb=1024,pmem_path='/mnt/pmem0',force_new=True)

s.x = np.ndarray((1000000,), dtype=np.uint8)

print(s.x)

print(colored(255,255,255,"OK!"))
