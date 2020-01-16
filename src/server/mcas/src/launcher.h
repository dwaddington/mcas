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
#ifndef __mcas_LAUNCHER_H__
#define __mcas_LAUNCHER_H__

#include <common/logging.h>
#include <sstream>
#include <string>

#include "config_file.h"
#include "program_options.h"
#include "shard.h"

namespace mcas {
class Shard_launcher {
public:
  Shard_launcher(Program_options &options) :
    _config_file(options.config_file)
  {
    for (unsigned i = 0; i < _config_file.shard_count(); i++) {
      PMAJOR("launching shard: core(%d) port(%d) net(%s) (forced-exit=%s)", _config_file.get_shard_core(i),
             _config_file.get_shard_port(i), _config_file.get_shard("net", i).c_str(), options.forced_exit ? "y":"n");

      auto dax_config = _config_file.get_shard_dax_config(i);
      std::string dax_config_json;

      /* handle DAX config if needed */
      if (dax_config.size() > 0) {
        std::stringstream ss;
        ss << "[{\"region_id\":0,\"path\":\"" << dax_config[0].first
           << "\",\"addr\":\"" << dax_config[0].second << "\"}]";
        PLOG("DAX config %s", ss.str().c_str());
        dax_config_json = ss.str();
      }

      try {
        _shards.push_back(new mcas::Shard(_config_file,
                                          i, // shard index
                                          dax_config_json,
                                          options.debug_level,
                                          options.forced_exit));

      } catch (const std::exception &e) {
        PLOG("shard %d failed to launch: %s", i, e.what());
      }
    }
  }

  ~Shard_launcher() {
    PLOG("Exiting shard (%p)", this);
    for (auto &sp : _shards)
      delete sp;
  }

  void wait_for_all() {
    pthread_setname_np(pthread_self(), "launcher");
    bool alive;
    do {
      sleep(1);
      alive = false;
      for (auto &sp : _shards) {
        alive = !sp->exited();
      }
    } while (alive);
  }

private:
  Config_file                _config_file;
  std::vector<mcas::Shard *> _shards;
};
} // namespace mcas

#endif // __mcas_LAUNCHER_H__
