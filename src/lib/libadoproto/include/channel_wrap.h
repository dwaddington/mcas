/*
  Copyright [2017-2019] [IBM Corporation]
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __CHANNEL_WRAP_H__
#define __CHANNEL_WRAP_H__

#include "uipc.h"
#include <string>

class channel_wrap
{
  channel_t _ch;
public:
  channel_wrap(channel_t ch_)
    : _ch(ch_)
  {}
  channel_wrap()
    : channel_wrap(nullptr)
  {}
  channel_wrap(const channel_wrap &) = delete;
  channel_wrap &operator=(const channel_wrap &) = delete;
  ~channel_wrap()
  {
    close();
  }

  void open(const std::string &s);

  void create(const std::string &s, size_t message_size, size_t queue_size);

  void close();

  operator channel_t() const { return _ch; }
};

#endif
