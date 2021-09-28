#ifndef __PYMM_METADATA_H__
#define __PYMM_METADATA_H__

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct {
    uint32_t magic;
    uint32_t txbits;
    uint32_t version;
    uint32_t type;
    uint32_t subtype;
  } MetaHeader;

  static constexpr uint32_t HeaderMagic = 0xCAF0;
  static constexpr uint32_t HeaderSize = sizeof(MetaHeader);

  static constexpr uint32_t DataType_Unknown       = 0;
  static constexpr uint32_t DataType_Opaque        = 1;
  static constexpr uint32_t DataType_String        = 2;
  static constexpr uint32_t DataType_NumberFloat   = 3;
  static constexpr uint32_t DataType_NumberInteger = 4;
  static constexpr uint32_t DataType_Bytes         = 5;  
  static constexpr uint32_t DataType_NumPyArray    = 10;
  static constexpr uint32_t DataType_TorchTensor   = 11;
  static constexpr uint32_t DataType_DLPackArray   = 12;
  static constexpr uint32_t DataType_LinkedList    = 23;

  static constexpr uint32_t DataSubType_None   = 0;
  static constexpr uint32_t DataSubType_Ascii  = 10;
  static constexpr uint32_t DataSubType_Utf8   = 11;
  static constexpr uint32_t DataSubType_Utf16  = 12;
  static constexpr uint32_t DataSubType_Latin1 = 13;

  
#ifdef __cplusplus
}
#endif

#endif
