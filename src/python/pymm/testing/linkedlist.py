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
        log("Testing: pymm.linked_list append method")
        shelf.x.append(123) # will be stored inline
        shelf.x.append(1.321) # will be stored inline
        shelf.x.append(np.ndarray((3,3,),dtype=np.uint8)) # will be stored as item in index
        shelf.x.append("Hello list!") # will be stored as item in index
        shelf.x.append("Goodbye list!") # will be stored as item in index
        print(shelf.items)

    def test_C_list_access(self):
        global shelf
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
        

    def test_D_list_item_assignment(self):
        global shelf
        log("Testing: pymm.linked_list item assignment")
        shelf.x[0] = 999
        self.assertTrue(shelf.x[0] == 999)
        shelf.x[1] = np.ndarray(3,)
        shelf.x[1] = np.ndarray(4,)
        
    def test_E_list_len(self):
        global shelf
        print("Length of list:{}".format(len(shelf.x)))
        self.assertTrue(len(shelf.x) == 5)
        print(shelf.x)

    def xxxtest_E_list_iterate(self):
        for e in shelf.x:
            print(e)


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
