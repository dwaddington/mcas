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



uint64_t __builtin_bswap64 (uint64_t x);


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



item rows[100];




int main(int argc, char **argv)
{



	for (int i=0;i<100;i++) { 
		rows[0].obj = 0;
		rows[0].size = 0;
		rows[0].op_time.val[1] = 0;
		rows[0].op_time.val[0] = 0;
	}





/*
     rows[0].op_time.val[0] = 30170534 ;
     rows[1].op_time.val[0] = 38843579 ;
     rows[2].op_time.val[0] = 46012413 ;
     rows[3].op_time.val[0] = 32068522 ;
     rows[4].op_time.val[0] = 47970612 ;
     rows[5].op_time.val[0] = 44847683 ;
     rows[6].op_time.val[0] = 36685264 ;
     rows[7].op_time.val[0] = 39848079 ;
     rows[8].op_time.val[0] = 11428489 ;
     rows[9].op_time.val[0] = 56215521 ;
     rows[10].op_time.val[0] = 51091754 ;
     rows[11].op_time.val[0] = 52947943 ;
     rows[12].op_time.val[0] = 51761839 ;
     rows[13].op_time.val[0] = 57949539 ;
     rows[14].op_time.val[0] = 7564334 ;
     rows[15].op_time.val[0] = 8338466 ;
     rows[16].op_time.val[0] = 54293755 ;
     rows[17].op_time.val[0] = 9965970;
     rows[18].op_time.val[0] = 10309926 ;
     rows[19].op_time.val[0] = 33292820 ;
     rows[20].op_time.val[0] = 10115630 ;
     rows[21].op_time.val[0] = 8449863 ;
     rows[22].op_time.val[0] = 23203124 ;
     rows[23].op_time.val[0] = 45111157 ;
     rows[24].op_time.val[0] = 37461709 ;
     rows[25].op_time.val[0] = 6285890;
     rows[26].op_time.val[0] = 6104855 ;
     rows[27].op_time.val[0] = 45284649 ;
     rows[28].op_time.val[0] = 25480455 ;
     rows[29].op_time.val[0] = 46810057 ;
*/
     uint64_t num = 257;	
     printf ( "binary of number %lu blig %x little %lu \n ",num, __builtin_bswap64(num), __builtin_bswap64(__builtin_bswap64(num)));
     rows[0].op_time.val[0] = __builtin_bswap64(34);
     rows[1].op_time.val[0] = __builtin_bswap64(79);
     rows[2].op_time.val[0] = __builtin_bswap64(13);
     rows[3].op_time.val[0] = __builtin_bswap64(22);
     rows[4].op_time.val[0] = __builtin_bswap64(12);
     rows[5].op_time.val[0] = __builtin_bswap64(83);
     rows[6].op_time.val[0] = __builtin_bswap64(64);
     rows[7].op_time.val[0] = __builtin_bswap64(80);
     rows[8].op_time.val[0] = __builtin_bswap64(89);
     rows[9].op_time.val[0] = __builtin_bswap64(21);
     rows[10].op_time.val[0] = __builtin_bswap64(54);
     rows[11].op_time.val[0] = __builtin_bswap64(43);
     rows[12].op_time.val[0] = __builtin_bswap64(39);
     rows[13].op_time.val[0] = __builtin_bswap64(40);
     rows[14].op_time.val[0] = __builtin_bswap64(4);
     rows[15].op_time.val[0] = __builtin_bswap64(6);
     rows[16].op_time.val[0] = __builtin_bswap64(55);
     rows[17].op_time.val[0] = __builtin_bswap64(3);
     rows[18].op_time.val[0] = __builtin_bswap64(26);
     rows[19].op_time.val[0] = __builtin_bswap64(20);
     rows[20].op_time.val[0] = __builtin_bswap64(30);
     rows[21].op_time.val[0] = __builtin_bswap64(4);
     rows[22].op_time.val[0] = __builtin_bswap64(24);
     rows[23].op_time.val[0] = __builtin_bswap64(57);
     rows[24].op_time.val[0] = __builtin_bswap64(19);
     rows[25].op_time.val[0] = __builtin_bswap64(2);
     rows[26].op_time.val[0] = __builtin_bswap64(256);
     rows[27].op_time.val[0] = __builtin_bswap64(549);
     rows[28].op_time.val[0] = __builtin_bswap64(55);
     rows[29].op_time.val[0] = __builtin_bswap64(557);





     hot::singlethreaded::HOTSingleThreaded<ValueType, KeyExtractor> hot;

	for (uint64_t i = 0; i < 30; i++) {
		printf ("Insert key %lu key[0] %lu, key[1] %lu\n", &rows[i].op_time, rows[i].op_time.val[0], rows[i].op_time.val[1]);
		hot.insert(&(rows[i].op_time));
	}

	hot::singlethreaded::HOTSingleThreaded<ValueType, KeyExtractor>::const_iterator res = hot.lower_bound(rows[25].op_time);
	UUID sk = rows[2].op_time;
	printf ("search for key %lu key[0] %lu, key[1] %lu\n", &sk, sk.val[0], sk.val[1]);
	ValueType key = (ValueType) *res;
	printf ("res %lu key %lu key[0] %lu, key[1] %lu\n", res,key,  __builtin_bswap64(key->val[0]), __builtin_bswap64(key->val[1]));
	for (long i = 0; i < 29; i++) {
		++res;
		key = (ValueType) *res;
		if (key != 0) { 
			printf ("i=%lu ++res %lu key %lu key[0] %lu, key[1] %lu\n", i, res, key,  __builtin_bswap64(key->val[0]), __builtin_bswap64(key->val[1]));
			key = (ValueType) *res;
//			printf ("ptr+1 %lu \n", key);
		}
	}
//		if (res.mIsValid) {
//			res++;
//		}
	return 0;
}
