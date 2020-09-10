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
#ifndef __mcas_LAUNCHER_H__
#define __mcas_LAUNCHER_H__

#include <common/logging.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <sstream>
#include <string>

#include "config_file.h"
#include "program_options.h"
#include "shard.h"
#include <memory>

namespace mcas
{
class Shard_launcher {
  static constexpr const char *_cname = "Shard_launcher";
 public:
  Shard_launcher(Program_options &options) : _config_file(options.debug_level, options.config), _shards{}
  {
    for (unsigned i = 0; i < _config_file.shard_count(); i++) {
      auto net = _config_file.get_shard_optional("net", i);
      PMAJOR("launching shard: core(%d) port(%d) net(%s) (forced-exit=%s)", _config_file.get_shard_core(i),
             _config_file.get_shard_port(i), net ? net->c_str() : "<none>", options.forced_exit ? "y" : "n");

      std::ostringstream ss{};
      auto               dax_config = _config_file.get_shard_dax_config_raw(i);
      if (dax_config) {
        rapidjson::OStreamWrapper wrap(ss);
        for (rapidjson::Value &s : dax_config->GetArray()) {
          if (s.IsObject()) {
            auto it = s.FindMember("addr");
            if (it != s.MemberEnd() && it->value.IsString()) {
              auto addr = std::stoull(it->value.GetString(), nullptr, 0);
              it->value.SetUint64(addr);
            }
          }
        }

        rapidjson::Writer<rapidjson::OStreamWrapper> writer(wrap);
        dax_config->Accept(writer);
        PLOG("DAX config %s", ss.str().c_str());
      }
      try {
        _shards.push_back(std::make_unique<mcas::Shard>(
            _config_file, i  // shard index
            ,
            ss.str(), options.debug_level, options.forced_exit,
            options.profile_file_main.size() ? options.profile_file_main.c_str() : nullptr, options.triggered_profile));
      }
      catch (const std::exception &e) {
        PLOG("shard %d failed to launch: %s", i, e.what());
      }
      catch (const Exception &e) {
        PLOG("shard %d failed to launch: %s", i, e.cause());
      }
    }
  }

  ~Shard_launcher() { PLOG("%s::%s (%p)", _cname, __func__, static_cast<const void *>(this)); }

  bool threads_running()
  {
    for (auto &sp : _shards)
      if (!sp->exiting()) return true;

    return false;
  }

  void signal_shards_to_exit()
  {
    for (auto &sp : _shards) {
      sp->signal_exit();
    }
  }

  void wait_for_all()
  {
    pthread_setname_np(pthread_self(), "launcher");
    for (auto &sp : _shards) {
      try
      {
        sp->get_future();
      }
      catch ( const Exception &e )
      {
        PLOG("%s::%s: shard (%p): Exception %s",  _cname, __func__, static_cast<const void *>(this), e.cause());
      }
      catch ( const std::exception &e )
      {
        PLOG("%s::%s: shard (%p) std::exception %s",  _cname, __func__, static_cast<const void *>(this), e.what());
      }
    }
  }

  void send_cluster_event(const std::string& sender, const std::string& type, const std::string& content)
  {
    for (auto &sp : _shards) {
      sp->send_cluster_event(sender, type, content);
    }
  }

 private:
  /* Probably no reason to make _config_file a member. Used only in the
   * constructor */
  Config_file                               _config_file;
  std::vector<std::unique_ptr<mcas::Shard>> _shards;
};
}  // namespace mcas

#endif  // __mcas_LAUNCHER_H__
