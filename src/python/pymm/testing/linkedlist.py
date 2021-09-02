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

global shelf
force_new=True
class TestLinkedList(unittest.TestCase):


    def test_A_list_construction(self):
        global shelf
        shelf = pymm.shelf('myShelf',size_mb=2048,pmem_path='/mnt/pmem0',backend="hstore-cc",force_new=force_new)
        log("Testing: pymm.linked_list construction")
        shelf.x = pymm.linked_list()
        print(shelf.x)

    def test_B_list_append(self):
        global shelf
        # APPEND
        log("Testing: pymm.linked_list append method")
        shelf.x.append(123)
        shelf.x.append(1.321)
        shelf.x.append(np.ndarray((3,3,),dtype=np.uint8))
        shelf.x.append("Hello list!")  

    def test_C_list_access(self):
        global shelf
        # ELEMENT ACCESS
        log("Testing: pymm.linked_list access")
        print(shelf.x[0])
        print(shelf.x[1])
        print(shelf.x[2])

        print(shelf.x)
        self.assertTrue(shelf.x[0] == 123)
        print(shelf.x[1])
        print(type(shelf.x[1]))
        self.assertTrue(shelf.x[1] == 1.321)
            
        print(shelf.x[3])
        self.assertTrue(shelf.x[3] == "Hello list!")

        # LEN

        print("Length of list:{}".format(len(shelf.x)))
        self.assertTrue(len(shelf.x) == 4)
        print(shelf.x)


    def XXX_test_B_add_shelf_ndarray(self):
        log("creating an ndarray on shelf, then adding to list")
        shelf.n = pymm.ndarray((3,8,))
        shelf.x.append(shelf.n)
        print(shelf.x)
        
#        print(shelf.items)
#        shelf.x[3] = "Goodbye";
#        self.assertTrue(shelfy.x[3] == "Goodbye")
#


if __name__ == '__main__':
    unittest.main()
