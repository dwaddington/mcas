# test for devdax and hstore
#
# first run ...
# DAX_RESET=1 python3 -i ~/mcas/src/python/pymm/testing/devdax.py
#
# then re-run ..(as many times as you want, vars will increment)
#
# python3 -i ~/mcas/src/python/pymm/testing/devdax.py
#
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
    

s = pymm.shelf('myShelf',size_mb=1024,backend='hstore-cc',pmem_path='/dev/dax0.0')
print(s.items)

if 'x' in s.items:
    log('x is there: value={}'.format(s.x))
    s.x += 1.1
else:
    log('x is not there!')
    s.x = 1.0

if 'y' in s.items:
    log('y is there:')
    log(s.y)
    s.y += 1
else:
    log('y is not there!')
    s.y = pymm.ndarray((10,10,),dtype=np.uint32)
    s.y.fill(0)

print(s.items)
print(colored(255,255,255,"OK!"))


