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
    

#-- mainline --

s = pymm.shelf('myTestShelf2',size_mb=8,backend='mapstore')

s.x = pymm.ndarray((10,10,))
s.x.fill(33)

print(s.items)
print(s.x)
print(s.x._value_named_memory.addr())

log("OK!")
