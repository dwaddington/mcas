#include <cstring>
#include <string>
#include <unordered_map>
#include <random>
#include <map>
#include <fstream>
#include <iostream>

#include "HOTSingleThreaded.hpp"
//#include "objx/contenthelpers/IdentityKeyExtractor.hpp"
//#include "objx/contenthelpers/OptionalValue.hpp"


struct UUID {
  uint64_t val[2];

public:
  bool operator<(const UUID& rhs) const {
    if (val[1] == rhs.val[1])
      return val[0] < rhs.val[0];
    else
      return val[1] < rhs.val[1];
  }

  bool operator==(const UUID& other) const {
    return val[0] == other.val[0] && val[1] == other.val[1];
  }
};


typedef UUID  KeyType;  
typedef UUID* ValueType;  

typedef  struct {
	uint64_t obj;
	uint64_t  size;
	UUID  op_time;
} item;


/* row->obj 
template <typename Val> struct KeyExtractor {

	inline long operator()(Val const &ptr) const {
		item* row = (item *)ptr;
		return row->obj;
	}
};
*/

template <typename Val> struct KeyExtractor {

	inline UUID operator()(Val const &ptr) const {
		KeyType *key = (KeyType *)ptr;
		return *key;
	}
};



item rows[4];

int main(int argc, char **argv)
{
	hot::singlethreaded::HOTSingleThreaded<ValueType, KeyExtractor> hot;

	for (uint64_t i = 4; i > 0; i--) {
		rows[i].obj = i;
		rows[i].size = i*1000;
		rows[i].op_time = {i, i*1000};
		printf ("row %lu obj %lu size %lu ptr %lu\n", i, rows[i].obj, rows[i].size, &rows[i]);
		hot.insert(&(rows[i].op_time));
	}

	for (long i = 1; i < 3; i++) {
		hot::singlethreaded::HOTSingleThreaded<ValueType, KeyExtractor>::const_iterator res = hot.lower_bound(rows[i].op_time);
		ValueType key = (ValueType) *res;
		printf ("res %lu key %lu key[0] %lu, key[1] %lu\n", res,key,  key->val[0], key->val[1]);
		++res;
		key = (ValueType) *res;
		printf ("++res %lu key %lu key[0] %lu, key[1] %lu\n", res,key,  key->val[0], key->val[1]);
//		key = (ValueType) *res;
//		printf ("ptr+1 %lu \n", key);
	}
//		if (res.mIsValid) {
//			res++;
//		}
	return 0;
}
