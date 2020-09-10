/*
   Copyright [2019-2020] [IBM Corporation]
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

#ifndef CCPM_LIST_ITEM_H
#define CCPM_LIST_ITEM_H

#include <cassert>

namespace ccpm
{
	/*
	 * Doubly linked list.
	 */
	struct list_item
	{
	private:
		list_item *_prev;
		list_item *_next;
	public:
		list_item() : _prev(this), _next(this) {}
		list_item(const list_item &) = delete;
		list_item &operator=(const list_item &) = delete;

		/* forcibly remove from a list */
		void force_reset()
		{
			_prev = this;
			_next = this;
		}

		/* insert i after this item */
		void insert_after(list_item *i)
		{
			assert( ! i->is_in_list() );
			const auto n = this->_next;
			i->_next = n;
			i->_prev = this;
			this->_next = i;
			n->_prev = i;
			assert( i->is_in_list() );
		}

		/* insert this item before i */
		void insert_before(list_item *i)
		{
			assert( ! i->is_in_list() );
			const auto p = this->_prev;
			i->_prev = p;
			i->_next = this;
			this->_prev = i;
			p->_next = i;
			assert( i->is_in_list() );
		}

		void remove()
		{
			assert( is_in_list() );
			_prev->_next = _next;
			_next->_prev = _prev;
			_prev = this;
			_next = this;
			assert( ! is_in_list() );
		}

		list_item *prev() { return _prev; }
		const list_item *prev() const { return _prev; }
		list_item *next() { return _next; }
		const list_item *next() const { return _next; }

		/* for list head: empty check */
		bool empty() const { return this == _next; }
		/* for a list item: "empty" means it is not in a list */
		bool is_in_list() const { return ! empty(); }

		/* returns true if element e is in the list.
		 * "this" is assumed to be a list anchor;
		 * e is not tested for equality with this.
		 */
		bool contains(const list_item *e) const
		{
			for ( auto p = this ; p->next() != this ; p = p->next() )
			{
				if ( p->next() == e ) { return true; }
			}
			return false;
		}
	};
}

#endif
