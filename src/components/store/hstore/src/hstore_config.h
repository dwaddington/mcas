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


#ifndef _MCAS_HSTORE_CONFIG_H_
#define _MCAS_HSTORE_CONFIG_H_

/*
 *   USE_CC_HEAP 2: simple allocation using offsets from a large region obtained from dax_map (NOT TESTED)
 *   USE_CC_HEAP 3: AVL-based allocation using actual addresses from a large region obtained from dax_map
 *   USE_CC_HEAP 4: bitmap-based allocation from a large region; crash-consistent
 *
 */

#if defined MCAS_HSTORE_USE_CC_HEAP
#define USE_CC_HEAP MCAS_HSTORE_USE_CC_HEAP
#else
#define USE_CC_HEAP 3
#endif

#define THREAD_SAFE_HASH 0
#define PREFIX_STATIC "HSTORE %s %s:%d "
#define LOCATION_STATIC __func__, __FILE__, __LINE__
#define PREFIX PREFIX_STATIC "%p "
#define LOCATION LOCATION_STATIC, common::p_fmt(this)

/* JIRA DAWN-292 requested a compile-configured grain size, default 32MiB 1<<25 */
#if ! defined HSTORE_LOG_GRAIN_SIZE
#define HSTORE_LOG_GRAIN_SIZE DM_REGION_LOG_GRAIN_SIZE
#endif

/* timestamps are enabled to match mapstore. To disable, compile with -DENABLE_TIMESTAMPS=0 */

#if ! defined ENABLE_TIMESTAMPS
#define ENABLE_TIMESTAMPS 1
#endif

#endif
