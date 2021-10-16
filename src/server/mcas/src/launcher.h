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
  using shard_vector = std::vector<std::unique_ptr<mcas::Shard>>;
 public:
  Shard_launcher(Program_options &options);
  Shard_launcher(bool debug_level_, common::string_view config_, bool forced_exit_, const char *profile_file_main_, bool triggered_profile_);

  ~Shard_launcher();

  bool threads_running();

  void signal_shards_to_exit();

  void reap_completions();

  void send_cluster_event(const std::string& sender, const std::string& type, const std::string& content);

 private:
#if 0
  /* Probably no reason to make _config_file a member. Used only in the
   * constructor */
  Config_file                               _config_file;
#endif
  shard_vector _shards;
};
}  // namespace mcas

#endif  // __mcas_LAUNCHER_H__
