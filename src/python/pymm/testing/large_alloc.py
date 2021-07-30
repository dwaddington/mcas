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
    

pymm.enable_transient_memory(backing_directory='/tmp')

s = pymm.shelf('myShelf',size_mb=1024,backend="hstore-cc",force_new=True)

# create a large right-hand side expression
w = np.ndarray((500000000),dtype=np.uint8)
print(hex(id(w)))

# copy to shelf
s.w = w

# force clean up of w
del w
gc.collect()
gc.get_objects()

print(s.w._value_named_memory.addr())
