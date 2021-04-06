#ifndef __DATA_H__
#define __DATA_H__

#include <common/exceptions.h>
#include <common/logging.h>
#include <common/str_utils.h>
#include <common/utils.h> /* MiB */
#include <sys/mman.h>

class KV_pair {
 public:
  std::string key;
  void *      value;
  size_t      value_len;
  KV_pair() : key(), value(nullptr), value_len(0) {}

  KV_pair(const KV_pair &) = delete;
  KV_pair &operator=(const KV_pair &) = delete;

  size_t size() { return key.length() + value_len; }
};

class Data {
  size_t _num_elements;
  size_t _key_len;
  size_t _val_len;
  bool   _random;

 public:
  KV_pair *_data;
  std::size_t value_space_size;
  void *value_space;

  Data() : Data(0) {}
private:
  Data(size_t num_elements) : Data(num_elements, 0, 0, false) {}
public:
  Data(size_t num_elements, size_t key_len, size_t val_len
    , bool random
  ) try
    : _num_elements(num_elements), _key_len(key_len), _val_len(val_len), _random(random)
    , _data(nullptr), value_space_size(), value_space(nullptr)
  {
    initialize_data(new KV_pair[_num_elements]);
  }
  catch ( std::bad_alloc &e )
  {
    using namespace std::string_literals;
    throw std::runtime_error("Data initialization error: "s + e.what());
  }
public:
  Data(const Data &) = delete;
  Data &operator=(const Data &) = delete;

  ~Data() { delete[] _data; free(value_space); }

  auto begin() -> KV_pair * { return _data; }

  auto begin() const -> const KV_pair * { return _data; }

  auto end() -> KV_pair * { return _data + _num_elements; }

  auto end() const -> const KV_pair * { return _data + _num_elements; }
private:
  void initialize_data(KV_pair *data_)
  {
    /* val_len_rounded is val_len+8 (see the addition of 8 in the "random" case) rounded for alignment */
    auto val_len_rounded = (_val_len + 8 + 63)/64 * 64;
    value_space_size = val_len_rounded * _num_elements;
    value_space = ::aligned_alloc(MiB(2), value_space_size);
    if ( value_space == nullptr )
    {
      throw std::bad_alloc();
    }

    ::madvise(value_space, value_space_size, MADV_DONTFORK);

    PLOG("Initializing data: %zu key length, %zu value length, %zu elements....", _key_len, _val_len, _num_elements);

    _data = data_;

    for (size_t i = 0; i < _num_elements; ++i) {
      auto key = common::random_string(_key_len);

      _data[i].key   = key;
      auto val_len   = _random ? static_cast<size_t>(rand()) % _val_len + 8 : _val_len;
      auto ptr       = static_cast<char *>(value_space) + i * val_len_rounded;
      _data[i].value = ptr;
      _data[i].value_len = val_len;
    }

    PLOG("%d elements initialized, size %d.", int(_num_elements), int(_val_len));
  }
public:
  const char *key(size_t i) const
  {
    if (i >= _num_elements) throw General_exception("index out of bounds");
    return _data[i].key.c_str();
  }

  const std::string &key_as_string(size_t i) const
  {
    if (i >= _num_elements) throw General_exception("index out of bounds");
    return _data[i].key;
  }

  const char *value(size_t i) const
  {
    if (i >= _num_elements) throw General_exception("index out of bounds");
    return static_cast<const char *>(_data[i].value);
  }

  size_t key_len() const { return _key_len; }

  size_t value_len(size_t i) const { return _data[i].value_len; }

  size_t value_len() const { return _val_len; }

  size_t num_elements() const { return _num_elements; }

  size_t memory_size() const { return _num_elements * _key_len * _val_len; }
};

#endif
