import mcas
import sys
import numpy as np

def test_0():
    '''
    Test basic array writes
    '''
    import pymm
    import numpy as np
  
    # create new shelf (override any existing myShelf)
    #
    s = pymm.shelf('myShelf',1024,pmem_path='/mnt/pmem0',force_new=True)

    # create variable x on shelf (using shadow type)
    s.x = pymm.ndarray((1000,1000),dtype=np.uint8)

    if s.x.shape != (1000,1000):
        raise RuntimeException('demo: s.x.shape check failed')

    s.x[0] = 1
    if s.x[0] != 1:
        raise('Test 0: failure')

    if s.x[0][0] != 1:
        raise('Test 0: failure')

    s.x[0][0] = 2

    if s.x[0][0] != 2:
        raise('Test 0: failure')

    print('Test 0 OK!')

    

