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
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __CORE_UIPC_H__
#define __CORE_UIPC_H__

#include <common/types.h> /* status_t */
#include <stddef.h>

/* Expose as C because we want other programming languages to
   interface onto this API */

struct uipc_channel;
typedef struct uipc_channel* channel_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Channel is bi-directional, user-level, lock-free exchange of
 * fixed sized messages (zero-copy).  Receiving is polling-based (it
 * can be configured as sleeping. It does not define the message
 * protocol which can be Protobuf etc.  Channel is a lock-free FIFO
 * (MPMC) in shared memory for passing pointers together with a slab
 * allocator (also lock-free and thread-safe across both sides) for
 * fixed sized messages.  Both sides map to the same virtual
 * address; this is negogiated. 4M exchanges per second is typical.
 *
 */

/**
 * Create a channel and wait for client to connect
 *
 * @param path_name Unique name (e.g., /tmp/myChannel)
 * @param message_size Message size in bytes.
 * @param queue_size Max elements in FIFO
 *
 * @return Handle to channel or NULL on failure
 */
channel_t uipc_create_channel(const char* path_name, size_t message_size,
                              size_t queue_size);

/**
 * Connect to a channel
 *
 * @param path_name Name of channel (e.g., /tmp/myChannel)
 * @param
 *
 * @return Handle to channel or NULL on failure
 */
channel_t uipc_connect_channel(const char* path_name);

/**
 * Close a channel
 *
 * @param channel Channel handle
 *
 * @return S_OK or E_INVAL
 */
status_t uipc_close_channel(channel_t channel);

/**
 * Allocate a fixed size message
 *
 * @param channel Associated channel
 *
 * @return Pointer to message in shared memory or NULL on failure
 */
void* uipc_alloc_message(channel_t channel);

/**
 * Free message
 *
 * @param channel Associated channel
 * @param message Message
 *
 * @return
 */
status_t uipc_free_message(channel_t channel, void* message);

/**
 * Send a message
 *
 * @param channel Channel handle
 * @param data Pointer to data in channel memory
 *
 * @return S_OK or E_FULL
 */
status_t uipc_send(channel_t channel, void* data) __attribute__((warn_unused_result));

/**
 * Recv a message
 *
 * @param channel Channel handle
 * @param data_out If return S_OK, pointer to data popped off FIFO
 *
 * @return S_OK or E_EMPTY
 */
status_t uipc_recv(channel_t channel, void** data_out) __attribute__((warn_unused_result));
#ifdef __cplusplus
}
#endif

#endif
