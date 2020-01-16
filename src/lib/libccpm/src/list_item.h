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

#ifndef CCPM_LIST_ITEM_H__
#define CCPM_LIST_ITEM_H__

namespace ccpm
{
	/*
	 * Doubly linked list.
	 */
	class list_item
	{
		list_item *_prev;
		list_item *_next;
	public:
		list_item() : _prev(this), _next(this) {}
		list_item(const list_item &) = delete;
		list_item &operator=(const list_item &) = delete;

		/* insert i after this item */
		void insert_after(list_item *i)
		{
			const auto n = this->_next;
			i->_next = n;
			i->_prev = this;
			this->_next = i;
			n->_prev = i;
		}

		/* insert this item before i */
		void insert_before(list_item *i)
		{
			const auto p = this->_prev;
			i->_prev = p;
			i->_next = this;
			this->_prev = i;
			p->_next = i;
		}

		void remove()
		{
			_prev->_next = _next;
			_next->_prev = _prev;
		}

		list_item *prev() { return _prev; }
		const list_item *prev() const { return _prev; }
		list_item *next() { return _next; }
		const list_item *next() const { return _next; }

		bool empty() const { return this == _next; }

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
