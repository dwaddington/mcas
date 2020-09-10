#!/usr/bin/python3

from dm import dm
from install_prefix import install_prefix

class pem_cert(dm):
    """ certificate specification (in a config) """
    def __init__(self):
        dm.__init__(self, {
            "cert" : "%s/certs/mcas-cert.pem" % (install_prefix,)
        })

if __name__ == '__main__':
    print("pem_cert", pem_cert().json())
