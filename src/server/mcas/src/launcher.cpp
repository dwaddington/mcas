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

#include "launcher.h"
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include <sstream>
#include <string>

#include "config_file.h"
#include "program_options.h"
#include "shard.h"
#include <memory>

using Shard_launcher = mcas::Shard_launcher;

Shard_launcher::Shard_launcher(Program_options &options)
  : Shard_launcher(
      options.debug_level, options.config, options.forced_exit,
      options.profile_file_main.size() ? options.profile_file_main.c_str() : nullptr,
      options.triggered_profile)
{
}

Shard_launcher::Shard_launcher(bool debug_level_, common::string_view config_, bool forced_exit_, const char *profile_file_main_, bool triggered_profile_)
  : _shards{}
{
  Config_file config_file(debug_level_, config_);
  for (unsigned i = 0; i < config_file.shard_count(); i++) {
    auto net = config_file.get_shard_optional("net", i);
    PMAJOR("launching shard: core(%d) port(%d) net(%s) (forced-exit=%s)", config_file.get_shard_core(i),
           config_file.get_shard_port(i), net ? net->c_str() : "<none>", forced_exit_ ? "y" : "n");

    std::ostringstream ss{};
    auto               dax_config = config_file.get_shard_dax_config_raw(i);
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
      /*
       * A "Shard" isa "Shard_transport", which hasa "Fabric", so the Fabric is
       * constructed on this thread, not on the shard thread which uses it.
       */
      _shards.push_back(std::make_unique<mcas::Shard>(
          config_file, i  // shard index
          ,
          ss.str(), debug_level_, forced_exit_, profile_file_main_, triggered_profile_));
    }
    catch (const std::exception &e) {
      PLOG("shard %d failed to launch: %s", i, e.what());
    }
  }
}

Shard_launcher::~Shard_launcher()
{
  FLOGM("");
}

bool Shard_launcher::threads_running()
{
  return ! _shards.empty();
}

void Shard_launcher::signal_shards_to_exit()
{
  for (auto &sp : _shards) {
    sp->signal_exit();
  }
}

void Shard_launcher::reap_completions()
{
  auto it =
    std::remove_if(
      _shards.begin(), _shards.end()
     , [] (shard_vector::reference i) { return i->exiting(); }
    );
  _shards.erase(it, _shards.end());
}

void Shard_launcher::send_cluster_event(const std::string& sender, const std::string& type, const std::string& content)
{
  for (auto &sp : _shards) {
    sp->send_cluster_event(sender, type, content);
  }
}
