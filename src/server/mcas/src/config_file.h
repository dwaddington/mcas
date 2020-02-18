/*
   Copyright [2017-2020] [IBM Corporation]
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
#ifndef __MCAS_CONFIG_FILE_H__
#define __MCAS_CONFIG_FILE_H__

#define RAPIDJSON_PARSE_ERROR_NORETURN(parseErrorCode, offset) \
  throw ParseException(parseErrorCode, #parseErrorCode, offset)

#include <stdexcept>  // std::runtime_error

#include "rapidjson/error/error.h"  // rapidjson::ParseResult

struct ParseException
    : std::runtime_error
    , private rapidjson::ParseResult {
  ParseException(rapidjson::ParseErrorCode code, const char *msg, size_t offset)
      : std::runtime_error(msg), ParseResult(code, offset)
  {
  }
};

#include <assert.h>
#include <common/exceptions.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

static const char *k_typenames[] = {"Null", "False", "True", "Object", "Array", "String", "Number"};

class Config_exception : public Exception {
 public:
  Config_exception(int err) : Exception("General exception"), _err_code(err) {}

  Config_exception() : _err_code(E_FAIL) {}

  __attribute__((__format__(__printf__, 2, 0))) Config_exception(const char *fmt, ...)
      : Exception(""), _err_code(E_FAIL)
  {
    va_list args;
    va_start(args, fmt);
    char msg[255] = {0};
    vsnprintf(msg, 254, fmt, args);
    set_cause(msg);
  }
  status_t error_code() const { return _err_code; }

 private:
  status_t _err_code;
};

namespace mcas
{
class Config_file {
 private:
  static constexpr bool option_DEBUG     = true;
  static constexpr auto DEFAULT_PROVIDER = "verbs";

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++" // uninitializes: _shards, _net_providers, _ado_path, _resources, _security
  Config_file(const std::string &filename) : _doc()
  {
    if (option_DEBUG) PLOG("config_file: (%s)", filename.c_str());

    using namespace rapidjson;

    /* use file stream instead of istreamwrapper because of older Ubuntu 16.04
     */
    FILE *fp = fopen(filename.c_str(), "rb");
    if (fp == nullptr) throw Config_exception("configuration file open/parse failed");

    struct stat st;
    stat(filename.c_str(), &st);
    char *buffer = static_cast<char *>(malloc(st.st_size));

    try {
      FileReadStream is(fp, buffer, st.st_size);
      _doc.ParseStream(is);
    }
    catch (...) {
      throw Config_exception("configuration file open/parse failed");
    }
    free(buffer);
    fclose(fp);

    try {
      if (!_doc.HasMember("shards")) throw Config_exception("invalid configuration file");
      _shards = _doc["shards"];
    }
    catch (...) {
      throw Config_exception("bad JSON in configuration file (shards))");
    }

    PLOG("shards type:%s", k_typenames[_shards.GetType()]);
    if (!_shards.IsArray()) throw Config_exception("bad JSON: shard should be array");

    for (auto &m : _shards.GetArray()) {
      if (!m["core"].IsInt()) throw Config_exception("bad JSON: shards::core member not integer");
      if (!m["port"].IsInt()) throw Config_exception("bad JSON: shards::port member not integer");

      if (!m["net"].IsNull())
        if (!m["net"].IsString()) throw Config_exception("bad JSON: optional shards::net member not string");
    }
    if (option_DEBUG) {
      for (unsigned i = 0; i < shard_count(); i++) {
        PLOG("shard: core(%d) port(%d) net(%s)", get_shard_core(i), get_shard_port(i), get_shard("net", i).c_str());
      }
    }

    if (_doc.HasMember("resources")) { /* optional */
      try {
        _resources = _doc["resources"];
      }
      catch (...) {
        throw Config_exception("bad JSON in configuration file (resources))");
      }

      PLOG("resource type:%s", k_typenames[_resources.GetType()]);
      if (!_resources.IsObject()) throw Config_exception("bad JSON: resources should be {..}");
      if (!_resources["ado_cores"].IsString()) throw Config_exception("bad JSON: resources::core members not string");
      if (_resources.HasMember("ado_manager_core"))
        if (!_resources["ado_manager_core"].IsInt())
          throw Config_exception("bad JSON: optional resources::ado manager core member not int");
    }

    if (_doc.HasMember("security")) {
      try {
        _security = _doc["security"];
      }
      catch (...) {
        throw Config_exception("bad JSON in configuration file (security))");
      }
      if (!_security.IsObject()) throw Config_exception("bad JSON: security should be {..}");
      if (!_security.HasMember("cert") || !_security["cert"].IsString())
        throw Config_exception("bad JSON: optional security::cert missing or wrong type");
    }

    auto nit = _doc.FindMember("net_providers");
    if (nit != _doc.MemberEnd()) {
      /* Note rapidjson.org says that a missing operator[] will assert.
       * If true, the catch may not do much good.
       */
      rapidjson::Value &net_provider = nit->value;
      PLOG("net_providers type:%s", k_typenames[net_provider.GetType()]);

      if (!net_provider.IsString()) {
        throw Config_exception("bad JSON: net_providers should be string");
      }
      _net_providers = net_provider.GetString();
    }
    else {
      _net_providers = DEFAULT_PROVIDER;
    }

    auto ado_path = _doc.FindMember("ado_path");
    if (ado_path != _doc.MemberEnd()) {
      rapidjson::Value &ap = ado_path->value;
      if (!ap.IsString()) {
        throw Config_exception("bad JSON: ado_path should be string");
      }
      _ado_path = ap.GetString();
    }

    if (option_DEBUG) {
      PLOG("net_providers: %s%s", (nit == _doc.MemberEnd() ? "(default) " : " "), get_net_providers().c_str());
    }
  }
#pragma GCC diagnostic pop

  rapidjson::SizeType shard_count() const { return _shards.Size(); }

  std::string get_ado_cores() const
  {
    if (_resources.IsNull()) return "";
    return std::string(_resources["ado_cores"].GetString());
  }

  int get_ado_manager_core() const
  {
    if (_resources.IsNull()) return -1;
    if (!_resources.HasMember("ado_manager_core")) return -1;
    return _resources["ado_manager_core"].GetInt();
  }

  auto get_shard(rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard out of bounds");
    assert(_shards[i].IsObject());
    return _shards[i].GetObject();
  }

  std::string get_shard_ado_core(rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard out of bounds");
    assert(_shards[i].IsObject());
    auto shard = _shards[i].GetObject();
    if (!shard.HasMember("ado_core")) return "";
    return std::string(shard["ado_core"].GetString());
  }

  float get_shard_ado_core_nu(rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard out of bounds");
    assert(_shards[i].IsObject());
    auto shard = _shards[i].GetObject();
    if (!shard.HasMember("ado_core_number") && shard.HasMember("ado_core")) return -1;
    if (!shard.HasMember("ado_core_number")) return 1;
    return shard["ado_core_number"].GetFloat();
  }

  unsigned int get_shard_core(rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard out of bounds");
    assert(_shards[i].IsObject());
    auto shard = _shards[i].GetObject();
    return shard["core"].GetUint();
  }

  unsigned int get_shard_port(rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard out of bounds");
    assert(_shards[i].IsObject());
    auto shard = _shards[i].GetObject();
    return shard["port"].GetUint();
  }

  std::string get_shard(std::string field, rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard out of bounds");
    if (field.empty()) throw Config_exception("get_shard invalid field");
    auto shard = get_shard(i);
    if (!shard.HasMember(field.c_str())) return std::string();
    return std::string(shard[field.c_str()].GetString());
  }

  std::string get_net_providers() const { return _net_providers; }

  std::string get_ado_path() const { return _ado_path; }

  auto get_shard_ado_plugins(rapidjson::SizeType i) const
  {
    auto result = std::make_unique<std::vector<std::string>>();
    if (i > shard_count()) throw Config_exception("get_ado_plugins shard out of bounds");

    auto shard = get_shard(i);
    if (shard.HasMember("ado_plugins")) {
      if (!shard["ado_plugins"].IsArray()) throw Config_exception("ado_plugins should be array of strings");
      auto array = shard["ado_plugins"].GetArray();
      for (auto itr = array.Begin(); itr != array.End(); ++itr) {
        if (!itr->IsString()) throw Config_exception("ado_plugin element should be string");
        result->push_back(itr->GetString());
      }
    }
    return result;
  }

  auto get_shard_object(std::string name, rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard_object out of bounds");
    if (name.empty()) throw Config_exception("get_shard_object invalid name");
    auto shard = get_shard(i);
    if (!shard.HasMember(name.c_str()))
      throw Config_exception("get_shard_object: object (%s) does not exist", name.c_str());
    return shard[name.c_str()].GetObject();
  }

  std::vector<std::pair<std::string, std::string>> get_shard_dax_config(rapidjson::SizeType i) const
  {
    if (i > shard_count()) throw Config_exception("get_shard_dax_config out of bounds");

    std::vector<std::pair<std::string, std::string>> result;

    auto shard = get_shard(i);
    if (!shard.HasMember("dax_config")) return result;

    if (0 != ::strcmp(k_typenames[shard["dax_config"].GetType()], "Array"))
      throw Config_exception("dax_config attribute should be an array");

    for (auto &config : shard["dax_config"].GetArray()) {
      if (!config.HasMember("path") || !config.HasMember("addr") || !config["path"].IsString() ||
          !config["addr"].IsString())
        throw Config_exception("badly formed JSON: dax_config");
      auto new_pair = std::make_pair(config["path"].GetString(), config["addr"].GetString());
      result.push_back(new_pair);
    }
    return result;
  }

  std::string get_cert_path() const
  {
    return (!_doc.HasMember("security")) ? std::string() : std::string(_security["cert"].GetString());
  }

  unsigned int debug_level() const
  {
    if (_doc["debug_level"].IsNull()) return 0;
    return _doc["debug_level"].GetUint();
  }

 private:
  rapidjson::Document _doc;
  rapidjson::Value    _shards;
  std::string         _net_providers;
  std::string         _ado_path;
  rapidjson::Value    _resources;
  rapidjson::Value    _security;
};
}  // namespace mcas
#endif  // __mcas_CONFIG_FILE_H__
