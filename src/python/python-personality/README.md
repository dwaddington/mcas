# Experimental Python Personality

**This feature is still in testing.**

The Python Personality (PP) provides: i.) a client API to load and
save Python objects from the MCAS store, ii.) support for Python execution
in the ADO.  In the case of Numpy
arrays, zero-copy is possible (through the direct APIs).  For other
data structures, the standard pickling function is used.

## Running basic test

This test will produce an X-window and thus cannot be fully run from a text
only console.  Code for test is in api.py:

```
SERVER_IP=<your server IP address> python3
```

Type interactively:

```python
> import pymcas
> pymcas.test_image_0()
```
