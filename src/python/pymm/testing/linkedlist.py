#!/usr/bin/python3 -m unittest
#
# testing transient memory (needs modified Numpy)
#
import unittest
import pymm
import numpy as np

def colored(r, g, b, text):
    return "\033[38;2;{};{};{}m{} \033[38;2;255;255;255m".format(r, g, b, text)

def log(*args):
    print(colored(0,255,255,*args))

force_new=True
class TestLinkedList(unittest.TestCase):
    def setUp(self):
        global force_new
        self.s = pymm.shelf('myShelf',size_mb=2048,pmem_path='/mnt/pmem0',backend="hstore-cc",force_new=force_new)
        force_new=False

    def tearDown(self):
        del self.s
    

    def test_list_construction(self):

        log("Testing: pymm.linked_list construction")
        self.s.x = pymm.linked_list()
        print(self.s.x)

        log("Testing: pymm.linked_list append method")
        self.s.x.append(123)
        self.s.x.append(1.321)
        self.s.x.append(np.ndarray((3,3,),dtype=np.uint8))
    
        print(self.s.x[0])
        print(self.s.x[1])
        print(self.s.x[2])
        self.s.x.append("Hello list!")

        print(self.s.x)
        self.assertTrue(self.s.x[0] == 123)
        print(self.s.x[1])
        print(type(self.s.x[1]))
        self.assertTrue(self.s.x[1] == 1.321)
            
        print(self.s.x[3])
        self.assertTrue(self.s.x[3] == "Hello list!")

        print(self.s.items)
        self.s.x[3] = "Goodbye";
        self.assertTrue(self.s.x[3] == "Goodbye")
        print("Length of list:{}".format(len(self.s.x)))


if __name__ == '__main__':
    unittest.main()
