import pymm
import numpy as np
import math
import torch
import gc

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
    

# based on https://pytorch.org/tutorials/beginner/pytorch_with_examples.html
pymm.pymmcore.enable_transient_memory()

s = pymm.shelf('myShelf',size_mb=500*1024,pmem_path='/mnt/pmem0',backend="hstore-cc",force_new=True)

w = np.ndarray((1000000000),dtype=np.uint8)
s.w = w
#w = np.ndarray((1000),dtype=np.uint8)
print(hex(id(w)))

del w

gc.collect()
gc.get_objects()

print('get_objects() ok')
print(s.w._value_named_memory.addr())

print(colored(255,255,255,"OK!"))
