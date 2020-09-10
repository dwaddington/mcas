#!/usr/bin/python3

import json

class dm(object):
    """
    A class which holds a dict "value" and which can deep-merge that value with another dictionary value
    The rules for dee merging two dicts:
     - if the key exists in exactly one dict, use that key, value
     - if the key exists in both dicts
       - the two values must be the same type
       - if the values are scalars, use the second value
       - if he values are dicts, depp-merge the dicts
       - (not yet used anywhere) if the values are lists, extendsthe first list with the second
    """

    def __init__(self, value_={}):
        self._value = value_
    # deep merge of dm self with dm other
    def merge(self,other):
        if isinstance(other, dm):
            self.merge(other.value())
        elif isinstance(other, dict):
            for k,v in other.items():
                if k in self._value:
                    if type(v) != type(self._value[k]):
                        # not the same types: probably an error
                        raise TypeError("mismatch %s vs %s" % type(v) % type(self._value[k]))
                    if isinstance(v, dict):
                        # dictionaries: deep merge
                        self._value[k] = dm(self._value[k]).merge(v).value()
                    elif isinstance(v, list):
                        # lists: append
                        self._value[k].extend(v)
                    else:
                        # not a special case: replace
                        self._value[k] = v
                else:
                    # new element
                    self._value[k] = v
        else:
            raise TypeError("other is a %s not a dict" % type(other))
        return self

    def value(self):
        """ extract just the dict value """
        return self._value

    def json(self):
        """ provide the dict value in JSON """
        return json.dumps(self.value())
