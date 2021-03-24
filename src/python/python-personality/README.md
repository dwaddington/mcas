# Experiment Python Personality

** This feature is still in testing. **

The Python Personality (PP) provides i.) a client API to loading and
saving Python objects from the MCAS store.  In the case of Numpy
arrays, zero-copy is possible (through the direct APIs).  For other
data structures, the standard pickling function is used.

In addition, PP allows Python code to be sent from the client to the MCAS server to
allow Python-based near-data compute.

## Running basic test

This test will produce an X-window and thus cannot be run from a text
only console.  Code for test is in api.py:

```
SERVER_IP=<your server IP address> python3
```

Type interactively:

```python
> import pymcas
> pymcas.test_image_0()
```
