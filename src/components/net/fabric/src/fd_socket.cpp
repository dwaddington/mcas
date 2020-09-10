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


/*
 * Authors:
 *
 */

#include "fd_socket.h"

#include "system_fail.h"

#include <netinet/in.h>
#include <sys/socket.h> /* send */

#include <unistd.h> /* read */

#include <cerrno>
#include <stdexcept>

Fd_socket::Fd_socket()
  : Fd_open()
{}

Fd_socket::Fd_socket(int fd_)
  : Fd_open(fd_)
{
  if ( fd() < 0 )
  {
    throw std::logic_error("negative fd in Fd_socket::Fd_socket");
  }
}

void Fd_socket::send(const void *buf, std::size_t size) const
{
  auto r = ::send(fd(), buf, size, MSG_NOSIGNAL);
  if ( r < 0 )
  {
    auto e = errno;
    system_fail(e, "send");
  }
  if ( r == 0 )
  {
     system_fail(ECONNABORTED, "send");
  }
}

void Fd_socket::recv(void *buf, std::size_t size) const
{
  std::ptrdiff_t r;
  do
  {
     r = ::read(fd(), buf, size);
  } while (r == -1 && ( errno == EAGAIN || errno == EWOULDBLOCK ) );
  if ( r < 0 )
  {
    auto e = errno;
    system_fail(e, "recv (neg)");
  }
  if ( r == 0 )
  {
    system_fail(ECONNABORTED, "recv (zero)");
  }
}
