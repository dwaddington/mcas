from ctypes import *

# corresponds to core/metadata.h C definition

def construct_header(type=0, subtype=0, txbits=0, version=0):
    hdr = MetaHeader()
    hdr.magic = HeaderMagic
    hdr.txbits = txbits
    hdr.version = version
    hdr.type = type
    return hdr

def init_header_from_buffer(buffer: memoryview):
    hdr = MetaHeader.from_buffer(memref.buffer)
    hdr.magic = HeaderMagic
    hdr.txbits = 0
    hdr.version = 0
    return hdr

class MetaHeader(Structure):
    _fields_ = [("magic", c_uint),
                ("txbits", c_uint),
                ("version", c_uint),
                ("type", c_uint),
                ("subtype", c_uint)
    ]


HeaderSize = 20
HeaderMagic = int(202100001)

DataType_Unknown       = int(0)
DataType_Opaque        = int(1)
DataType_NumPyArray    = int(2)
DataType_TorchTensor   = int(4)
DataType_String        = int(8)
DataType_Bytes         = int(9)
DataType_NumberFloat   = int(21)
DataType_NumberInteger = int(22)
DataType_LinkedList    = int(23)

DataSubType_None   = int(0)
DataSubType_Ascii  = int(10)
DataSubType_Utf8   = int(11)
DataSubType_Utf16  = int(12)
DataSubType_Latin1 = int(13)
