#ifndef __DATA_H__
#define __DATA_H__

#include <sys/mman.h>
#include <common/str_utils.h>
#include <common/logging.h>
#include <common/exceptions.h>

class KV_pair 
{
public:
  std::string key;
  void* value;
  size_t value_len;
  KV_pair()
    : key(), value(nullptr), value_len(0)
  {}

  size_t size() { return key.length() + value_len; }
};

class Data
{
  size_t _num_elements;
  size_t _key_len;  
  size_t _val_len; 

public:
  KV_pair * _data;
  
  Data() 
    : Data(0)
  {
  }
 
  Data(size_t num_elements)
    : Data(num_elements, 0, 0, true)
  {
  }

  Data(size_t num_elements, size_t key_len, size_t val_len, bool delay_initialization=false)
    : _num_elements(num_elements)
    , _key_len(key_len)
    , _val_len(val_len)
    , _data(nullptr)
  {
    if (!delay_initialization)
    {
      initialize_data(new KV_pair[_num_elements]);
    }
  }

  Data(const Data &) = delete;
  Data& operator=(const Data &) = delete;

  ~Data() 
  {
    delete [] _data;
  }

  auto begin() -> KV_pair *
  {
    return _data;
  }

  auto begin() const -> const KV_pair *
  {
    return _data;
  }

  auto end() -> KV_pair *
  {
    return _data + _num_elements;
  }

  auto end() const -> const KV_pair *
  {
    return _data + _num_elements;
  }

  void initialize_data(KV_pair *data)
  {
    PLOG("Initializing data: %d key length, %d value length, %d elements....",
         int(_key_len), int(_val_len), int(_num_elements));
        
    _data = data;
    
    for( size_t i=0; i<_num_elements; ++i) 
    {
      auto key = Common::random_string(_key_len);

      _data[i].key = key;
      auto ptr = ::aligned_alloc(64, _val_len);
      _data[i].value = ptr;
      madvise(ptr, _val_len, MADV_DONTFORK);
      _data[i].value_len = _val_len;
    }

    PLOG("%d elements initialized, size %d.", int(_num_elements), int(_val_len));
  }
  
  const char * key(size_t i) const 
  {
    if(i >= _num_elements) throw General_exception("index out of bounds");
    return _data[i].key.c_str();
  }
  
  const std::string & key_as_string(size_t i) const
  {
    if(i >= _num_elements) throw General_exception("index out of bounds");
    return _data[i].key;
  }

  const char * value(size_t i) const 
  {
    if(i >= _num_elements) throw General_exception("index out of bounds");
    return static_cast<const char *>(_data[i].value);
  }
  
  size_t key_len() const { return _key_len; }
  
  size_t value_len() const { return _val_len; }
  
  size_t num_elements() const { return _num_elements; }

  size_t memory_size() const { return _num_elements * _key_len * _val_len; }
};

#endif
