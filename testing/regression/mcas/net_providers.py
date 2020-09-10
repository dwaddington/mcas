#!/usr/bin/python3

from dm import dm

class verbs(dm) :
    """ verbs provider """
    def __init__(self):
        dm.__init__(self, {
            "net_providers": "verbs"
        })

class sockets(dm) :
    """ verbs provider """
    def __init__(self):
        dm.__init__(self, {
            "net_providers": "sockets"
        })

if __name__ == '__main__':
    print("verbs: ", verbs().json())
    print("sockets: ", sockets().json())
