#include <cstring>
#include <string>
#include <unordered_map>
#include <random>
#include <map>
#include <fstream>
#include <iostream>

#include "HOTSingleThreaded.hpp"
//#include "idx/contenthelpers/IdentityKeyExtractor.hpp"
//#include "idx/contenthelpers/OptionalValue.hpp"

struct ROW {
	long id;
	char name[32];

	ROW() {
		id = 0;
		memset(name, 0, sizeof(name));
	}

/*
	bool operator<(const KEY& rhs) const {
		return memcmp(x, rhs.x, sizeof(x)) < 0;
	}

	bool operator==(const KEY& other) const {
		return memcmp(x, other.x, sizeof(x)) == 0;
	}

	friend std::ostream& operator<<(std::ostream& os, const KEY& k) {
		return os << k.x;
	}
	*/
};

template <typename Val> struct KeyExtractor {

	inline long operator()(Val const &ptr) const {
		ROW* row = (ROW *)ptr;
		return row->id;
	}
};

ROW rows[4];

int main(int argc, char **argv)
{
	hot::singlethreaded::HOTSingleThreaded<ROW*, KeyExtractor> hot;

	for (long i = 0; i < 4; i++) {
		rows[i].id = i;
		sprintf(rows[i].name, "Person # %ld", i);
	}

	for (long i = 0; i < 4; i++) {
		hot.insert(&rows[i]);
		auto res = hot.lookup(i);
		if (res.mIsValid) {
			ROW* row = (ROW *) res.mValue;
			printf("%ld -> %s\n", row->id, row->name);
		}
	}
	return 0;
}
