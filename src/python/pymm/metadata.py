from ctypes import *

# corresponds to core/metadata.h C definition

def construct_header(type=0, subtype=0, txbits=0, version=0):
    hdr = MetaHeader()
    hdr.magic = HeaderMagic
    hdr.txbits = txbits
    hdr.version = version
    hdr.type = type
    return hdr

def construct_header_on_buffer(buffer, type=0, subtype=0, txbits=0, version=0):
    hdr = MetaHeader.from_buffer(buffer)
    hdr.magic = HeaderMagic
    hdr.txbits = txbits
    hdr.version = version
    hdr.type = type
    return hdr


def init_header_from_buffer(buffer: memoryview):
    hdr = MetaHeader.from_buffer(buffer)
    hdr.magic = HeaderMagic
    hdr.txbits = 0
    hdr.version = 0
    return hdr

def construct_header_from_buffer(buffer: memoryview):
    hdr = MetaHeader.from_buffer(buffer)
    if hdr.magic != HeaderMagic:
        raise RuntimeError('bad magic: {} construct header from buffer'.format(hex(hdr.magic)))
    return hdr

class MetaHeader(Structure):
    _fields_ = [("magic", c_uint32),
                ("txbits", c_uint32),
                ("version", c_uint32),
                ("type", c_uint32),
                ("subtype", c_uint32)
    ]


HeaderSize = 20
HeaderMagic = int(0xCAF0)

DataType_Unknown       = int(0)
DataType_Opaque        = int(1)
DataType_String        = int(2)
DataType_NumberFloat   = int(3)
DataType_NumberInteger = int(4)
DataType_Bytes         = int(5)
DataType_NumPyArray    = int(10)
DataType_TorchTensor   = int(11)
DataType_LinkedList    = int(23)

DataSubType_None   = int(0)
DataSubType_Ascii  = int(10)
DataSubType_Utf8   = int(11)
DataSubType_Utf16  = int(12)
DataSubType_Latin1 = int(13)
