
import pickle
import mcas as mapi
import numpy as np

def put_ndarray(pool, key, array):
    '''
    Put a Numpy ndarray into MCAS pool
    '''
    if not isinstance(key, str):
        raise Exception('invalid key parameter')
    if not isinstance(array, np.ndarray):
        raise Exception('invalid array parameter')

    # we could decided to combine the value and the
    # metadata to a single key slot, but at the momemnt we
    # do not have a invoke_ado_put that support zero-copy (direct)
    # transfer
    
    # first put the ndarray data into the store using zero-copy
    # we derive the name from the key
    pool.put_direct(key + '-data' , array)

    # then the metadata will be attached to the key
    metadata = (array.shape, mapi.get_type_num(array), array.nbytes)

    pool.put_direct(key, pickle.dumps(metadata))

def get_ndarray(pool, key):
    '''
    Get a Numpy ndarray from MCAS pool
    '''
    if not isinstance(key, str):
        raise Exception('invalid key parameter')

    # retrieve the metadata
    (shape, type_num, nbytes) = pickle.loads(bytes(pool.get_direct(key)))

    # now retrieve the data
    return pool.get_direct(key + '-data', shape=shape, type_num=type_num, nbytes=nbytes)

def invoke_ndarray(pool, key, operation):
    '''
    Invoke ADO operation on value
    '''
    if not isinstance(key, str):
        raise Exception('invalid key parameter')
    if not isinstance(operation, str):
        raise Exception('invalid operation parameter')

    return pool.invoke_ado(key, operation)

#--------------------------------------------------------------------------------

def testsession():
    poolname = 'testpool'
    session = mapi.Session(ip="10.0.0.101", port=11911)
    pool = session.create_pool(poolname,int(2e9),100)
    return (poolname, session, pool)

def test():
    (poolname, session, pool) = testsession()
    key = 'record000'
    data = np.identity(5,dtype=np.float32)
    print(data)
    put_ndarray(pool, key, data)
    # in-place only for the moment 
    program='matrix[0,0]+=3 ; matrix[1,1]+=3 ; matrix[2,2]+=3'
    r = invoke_ndarray(pool, key, program)
    print(r)
    r = get_ndarray(pool, key)
    pool.close()
    print(r)
    return r

