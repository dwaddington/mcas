/*
   Copyright [2019] [IBM Corporation]
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

#include "atomic_word.h"

unsigned ccpm::aw_count_max_free_run(atomic_word aw)
{
	/* A bit is free if its value is zero.
	 */

	/* Algorithm of Hacker's Delight 6-3; modified to count runs of 0s, not 1s */
	aw = ~aw;

	unsigned k = 0;
	for ( ; aw != 0x0; ++k )
	{
		aw = aw & (aw << 1);
	}
	return k;
}
