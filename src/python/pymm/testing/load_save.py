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
    

s = pymm.shelf('myShelf',size_mb=2048,backend="hstore-cc",force_new=True)

# create a large right-hand side expression which will be evaluated in pmem transient memory
s.w = np.ndarray((10000),dtype=np.uint8) # 1GB
s.w.fill(1)
np.save("savedSnapshot", s.w)

# do something that needs undoing
s.w.fill(3)

# undo
s.w = np.load("savedSnapshot.npy")

if np.all((s.w == 1)):
    log('recovery is correct!')
else:
    print_error('snapshot is incorrect! eek!')


print(s.w)
