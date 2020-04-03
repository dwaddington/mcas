# Python API

MCAS supports a Python-based client access.  This can be accessed through the 'mcas-shell' program or included
directly in a Python program.

Note: make sure dist/lib is added to the library path (LD_LIBRARY_PATH)

## Run shell (e.g.):

```
./dist/bin/mcas-shell
```

## Open session to remote MCAS server/shard

```python
session = mcas.Session(ip="10.0.0.21", port=11911)
```

## Create pool

```python
pool = session.create_pool("myPool",int(2e6),100)
```

## Open pool

```python
pool = session.open_pool("myPool")
```

## Put key-value pair

Key parameter is a string.  Value parameter is a string or a byte array.

```python
pool.put('key9','The Cute Little Dog')
```

## Put direct

Direct, zero-copy put can be applied to byte arrays

```python
pickled_item = pickle.dumps(item)
pool.put_direct(keyname, bytearray(pickled_item))
```

## Get key-value pair

```python
myval = pool.get('key9')
```

Direct get:

```python
bytearray_item = pool.get_direct(keyname)
return pickle.loads(bytes(bytearray_item))
```

## Key scanning

If the index component is loaded, then keys can be 'searched':

```python
print(get_keys(pool, "regex:.*"))
print(get_keys(pool, "next:"))
print(get_keys(pool, "prefix:tre"))
```

Example function to get list of keys:

```python
def get_keys(pool, expr):
    result = []
    offset=0
    print(expr)
    (k,offset)=pool.find_key(expr, offset)
    while k != None:
        result.append(k)
        offset += 1
        (k, offset) = pool.find_key(expr, offset)
    return result
```

## Value Attributes

Length and crc32 attributes can be fetched:

```python
print('Len: %d' % pool.get_attribute('array0','length'))
print('Crc: %x' % pool.get_attribute('array0','crc32'))
```

## Pool Attributes

```python
print('Size enquiry:%d' % pool.get_size('array0'))
```

Get number of objects in a pool:

```python
print(pool.count())
```

## Close pool

```python
pool.close()
```
