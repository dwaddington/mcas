/*
   Copyright [2020] [IBM Corporation]
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


/*
 * Authors:
 *
 */

#ifndef _NUPM_OPENED_SPACE_H_
#define _NUPM_OPENED_SPACE_H_

#include <common/fd_locked.h>
#include <common/logging.h> /* log_source */
#include <common/memory_mapped.h>
#include <common/moveable_ptr.h>
#include <common/types.h> /* addr_t */
#include <cstddef>
#include <string>
#include <vector>
#include <sys/uio.h> /* iovec */

namespace nupm
{
struct dax_manager;

struct range_use
{
private:
  common::moveable_ptr<dax_manager> _dm;
  /* Note: arena_fs may someday use multiple ranges */
  std::vector<common::memory_mapped> _iovm;

  std::vector<common::memory_mapped> address_coverage_check(std::vector<common::memory_mapped> &&iovm);
public:
  ::iovec iov(std::size_t i) { return _iovm[i].iov(); }
  range_use(dax_manager *dm_, std::vector<common::memory_mapped> &&);
  range_use(const range_use &) = delete;
  range_use &operator=(const range_use &) = delete;
  range_use(range_use &&) noexcept = default;
  ~range_use();
};

struct space_opened : private common::log_source
{
  common::fd_locked _fd_locked;
  /* Note: arena_fs may someday use multiple ranges */
  range_use _range;

  /* owns the file mapping */
  std::vector<common::memory_mapped> map_dev(int fd, const addr_t base_addr);
  std::vector<common::memory_mapped> map_fs(int fd, const std::vector<::iovec> &mapping);
public:
  space_opened(const common::log_source &, dax_manager * dm_, const std::string &path, const addr_t base_addr);
  space_opened(const common::log_source &, dax_manager * dm_, const std::string &path, const std::vector<::iovec> &mapping);
  space_opened(const common::log_source &, dax_manager * dm_, common::fd_locked && fd, const std::string &path, const std::vector<::iovec> &mapping);
  space_opened(space_opened &&) noexcept = default;
};
}
#endif
