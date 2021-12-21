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

#include "cpp_rangeindex_plugin.h"
#include <libpmem.h>
#include <api/interfaces.h>
#include <common/logging.h>
#include <common/dump_utils.h>
#include <common/type_name.h>
#include <sstream>
#include <string>
#include <list>
#include <algorithm>
#include <ccpm/immutable_list.h>
#include <ccpm/immutable_string_table.h>
#include "cpp_rangeindex_types.h"
#include <btree_multimap.h>
#include <blindi_btree_hybrid_nodes_multimap.h>
#include <HOTSingleThreaded.hpp>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
//#define DEBUG_PLUGIN 

#define ELASTIC 
//#define HOT 


using namespace rangeindex_ADO_protocol;
using namespace ccpm;
using namespace stx;
using namespace std;
using namespace boost;

#define RANGE_NUM 10

#define END 16000


uint64_t __builtin_bswap64 (uint64_t x);


char *str1 = "string Literal";
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
struct GenericIndex {
public:
    virtual std::string name(void) = 0;
    virtual void PrintStat(void) {}
    virtual void UpdateThreadLocal(int num_threads) {}
    virtual void AssignGCID(int thread_id) {}
    virtual void UnregisterThread(int thread_id) {}
    virtual void PrintStats(void) {}
};



template<typename KeyType, typename ValueType>
struct Index : GenericIndex {
public:
    virtual bool Insert(const KeyType& key, const ValueType& value) = 0;
    virtual void GetValue(const KeyType &key,
                          std::vector<ValueType> &value_list)  = 0;

    virtual int  GetExact(const KeyType &key) = 0;

    virtual void PrintTreeStats()  = 0;

    virtual uint64_t GetRange(const KeyType &key)  = 0;


    virtual uint64_t GetRangeTime(const KeyType &key,
                  std::vector<ValueType> &value_list, uint64_t time_end) = 0;
};

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


uint64_t char8B_to_uint64 (const char * input) { 
	uint64_t out;
        out = input[0]<<(8*7) | input[1]<<(8*6)
		| input[2]<<(8*5)
		| input[3]<<(8*4)
		| input[4]<<(8*3)
		| input[5]<<(8*2)
		| input[6]<<(8*1)
		| input[7];
	return out;
}



//////////////  STX tree      /////////////////////////////////////////

template<typename KeyType, typename ValueType, int InnerSlots, int LeafSlots>
struct BTreeType : Index<KeyType, ValueType> {
    btree_multimap<KeyType, ValueType, InnerSlots, LeafSlots> tree;

public:
    std::string name(void) { return std::string("BTree") + "-" + std::to_string(InnerSlots) + "-" + std::to_string(LeafSlots); }

    bool Insert(const KeyType& key, const ValueType& value) {
        tree.insert(key, value);
        return true;
    }

    void GetValue(const KeyType &key,
                  std::vector<ValueType> &value_list) {
        auto it = tree.find(key);
        if (it != tree.end())
            value_list.push_back(it->second);
    }

   int GetExact(const KeyType &key) {
        auto it = tree.find(key);
        if (it != tree.end())
            return 1;
	return 0;
   }
   

     uint64_t GetRange(const KeyType &key) {
        auto it = tree.lower_bound(key);
        for (int i=0; i < 3; i++) {
		if (it == tree.end())
			break;
//		value_list.push_back(it->second);
		it++;
	}
    }
};

////////////////// Elastic /////////////////
template<typename KeyType, typename ValueType, template<typename, int> typename BlindiBtreeHybridType, int InnerSlots, int LeafSlots>
struct BlindiBTreeHybridNodes : Index<KeyType, ValueType> {
	    blindi_btree_hybrid_nodes_multimap<KeyType, ValueType, BlindiBtreeHybridType, InnerSlots, LeafSlots> tree;



std::string blindi_type;

public:
    BlindiBTreeHybridNodes(std::string type) : blindi_type(type) {}

    std::string name(void) { return blindi_type + "-" + std::to_string(InnerSlots) + "-" + std::to_string(LeafSlots); }

    bool Insert(const KeyType& key, const ValueType& value) {
	    tree.insert(key, value);
	    return true;
    }

    void PrintTreeStats (){
	    auto stats = tree.get_stats();
	    uint64_t capacity = 0.0;
	    uint64_t breathing_capacity = 0.0;
	    uint64_t insert_breath = breathing_count;
	    breathing_capacity = sizeof(uint8_t*) * breathing_sum;
	    capacity = stats.indexcapacity + breathing_capacity;
	    std::cout   << "STX Seqtree stats:\n";
	    std::cout   << "* leaf size:    " << stats.leafsize << std::endl 	
                    << "* leaf num:     " << stats.leaves << std::endl
                    << "* leaf slots:   " << stats.leafslots << std::endl
                    << "* inner size:   " << stats.nodesize << std::endl
                    << "* inner num:    " << stats.innernodes << std::endl
                    << "* inner slots:  " << stats.innerslots << std::endl
                    << "* theoretical index size: " << (stats.leaves * stats.leafsize) + (stats.innernodes * stats.nodesize) << std::endl
                    << "* index capacity:         " << stats.indexcapacity << std::endl
                    << "* overall item count:     " << stats.itemcount << std::endl
	            << " breathing_sum: " << breathing_sum << ", breathing_capacity " << breathing_capacity << std::endl  
 	            << " total_capacity: " << capacity/1024/1024.0  << " [MB] "  << std::endl
                    << " avgBYTE: " << 1.0 * capacity/ stats.itemcount << std::endl
		    << " insert_breathing: " << insert_breath <<  std::endl;
    }

    void GetValue(const KeyType &key,
                  std::vector<ValueType> &value_list) {

        auto it = tree.lower_bound(key);
	
#ifdef DEBUG_PLUGIN
	std::cout << " it.currslot " << it.currslot << std::endl;
#endif
        if (it.currslot != END) {
            value_list.push_back(it->second);
	}
	else {
#ifdef DEBUG_PLUGIN
		std::cout << " GetValue tree.end " << std::endl;
#endif
	}

    }


    int GetExact(const KeyType &key) {

        auto it = tree.lower_bound(key);
#ifdef DEBUG_PLUGIN
	std::cout << " it.currslot " << it.currslot << std::endl;
#endif
        if (it.currslot != END) {
		return 1;
	}
	return 0;
    }



    uint64_t GetRange(const KeyType &key) {
        auto it = tree.lower_bound(key);
	uint64_t sum = it->first.val[0];
	printf (" key[0] %lu, key[1] %lu\n", it->first.val[0], it->first.val[1]);
	for (int i=1; i < RANGE_NUM; i++) {
		if (it.currslot == ENDSLOT) {
			std::cout << " i=" << i << " Range tree.end END= " << ENDSLOT << std::endl;

			return sum;
		}
		it++;
		sum+=it->first.val[0];
	        printf (" key[0] %lu, key[1] %lu\n", it->first.val[0], it->first.val[1]);
	}
	return sum;
    }

    uint64_t GetRangeTime(const KeyType &key,
                  std::vector<ValueType> &value_list, uint64_t time_end) {

        auto it = tree.lower_bound(key);
	uint64_t cnt = 0;
        KeyType *curr_ts;
/*	
#ifdef DEBUG_PLUGIN
	std::cout << " current timestamp " << std::endl;
#endif	
	while  (it.currslot != ENDSLOT) {
#ifdef DEBUG_PLUGIN
		std::cout << " current slot " << it.currslot << std::endl;
#endif	
		curr_ts =  it->second;
		if (key.val[1] != curr_ts->val[1]){
#ifdef DEBUG_PLUGIN
			std::cout << "GetRangeTime the type are not identical key.type " << key.val[1] << " curr_ts " << curr_ts->val[1] << std::endl; 
#endif	
			return cnt;
		}	
		if (curr_ts->val[0] > time_end){
#ifdef DEBUG_PLUGIN
			std::cout << "GetRangeTime the curr-time is above the time ; curr_ts " << curr_ts->val[0] << " time_end " << time_end << std::endl; 
#endif	
			return cnt;
		}
		if (curr_ts->val[0] >= key.val[0]){
#ifdef DEBUG_PLUGIN
			std::cout << "GetRangeTime the curr-time in the right range count++ ; curr_ts " << curr_ts->val[0] << " key->val[0] " << key.val[0] << std::endl;
#endif	
		       cnt++;	
		}
	       	else {
#ifdef DEBUG_PLUGIN
			std::cout << "GetRangeTime the curr-time is below  count stay the same ; curr_ts " << curr_ts->val[0] << " key->val[0] " << key.val[0] << std::endl; 
#endif	
		}

		value_list.push_back(it->second);
		it++;
#ifdef DEBUG_PLUGIN
		std::cout << " current slot  after ++ " << it.currslot << std::endl;
#endif	

	}
	return cnt;
*/
    }

};


/////////////

const int INNER_NUM = 16;	
const int LEAF_NUM = 16;
//typedef string  KeyType;  
//typedef const char*  ValueType;  
typedef UUID  KeyType;  
typedef UUID*  ValueType;  
typedef uint64_t  KeyType2;  
typedef uint64_t*  ValueType2;  
//Index<KeyType, ValueType> *idx = new BTreeType<KeyType, ValueType, INNER_NUM, LEAF_NUM>; // BTREE
Index<KeyType, ValueType> *idx1 = new BlindiBTreeHybridNodes<KeyType, ValueType, SeqTreeBlindiNode, INNER_NUM, LEAF_NUM>("SeqTreeBlindi"); // ELASTIC 
//Index<KeyType2, ValueType2> *idx2 = new BlindiBTreeHybridNodes<KeyType2, ValueType2, SeqTreeBlindiNode, INNER_NUM, LEAF_NUM>("SeqTreeBlindi"); // ELASTIC HOT

/////////////////////////// 
// HOT

template <typename Val> struct KeyExtractor {

	inline KeyType operator()(Val const &ptr) const {
		KeyType *key = (KeyType *)ptr;
		return *key;
	}
};

hot::singlethreaded::HOTSingleThreaded<ValueType, KeyExtractor> hot1;


//
//////////////////////////////
typedef  struct {
	UUID  obj_time;
	uint64_t op;
	uint64_t  size;
} item;

item table[48000000]; 
uint64_t row_nu = 0;



///////////////////////////////////////////////////////


status_t ADO_rangeindex_plugin::register_mapped_memory(void * shard_vaddr,
                                                   void * local_vaddr,
                                                   size_t len) {
  PLOG("ADO_rangeindex_plugin: register_mapped_memory (%p, %p, %lu)", shard_vaddr,
       local_vaddr, len);

  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */
  return S_OK;
}

void ADO_rangeindex_plugin::launch_event(const uint64_t auth_id,
                                     const std::string& pool_name,
                                     const size_t pool_size,
                                     const unsigned int pool_flags,
                                     const unsigned int memory_type,
                                     const size_t expected_obj_count,
                                     const std::vector<std::string>& params)
{
}


status_t ADO_rangeindex_plugin::do_work(const uint64_t work_request_id,
                                    const char * key,
                                    size_t key_len,
                                    IADO_plugin::value_space_t& values,
                                    const void * in_work_request, /* don't use iovec because of non-const */
                                    const size_t in_work_request_len,
                                    bool new_root,
                                    response_buffer_vector_t& response_buffers)
{
  using namespace flatbuffers;
  using namespace rangeindex_ADO_protocol;
  using namespace cpp_rangeindex_personality;

  auto value = values[0].ptr;

  constexpr size_t buffer_increment = 32*1024; /* granularity for memory expansion */

  //  PLOG("invoke: value=%p value_len=%lu", value, value_len);

#ifdef DEBUG_PLUGIN
  std::cout << "root " << std::endl;
#endif
  auto& root = *(new (value) Value_root);
  bool force_init = false;

  if(root.num_regions == 0) {
    void * buffer;
    if(cb_allocate_pool_memory(buffer_increment, 8, buffer)!=S_OK)
      throw std::runtime_error("unable to allocate new region");
    root.add_region(buffer, buffer_increment);
    force_init = true;
  }

  Verifier verifier(static_cast<const uint8_t*>(in_work_request), in_work_request_len);
  if(!VerifyMessageBuffer(verifier)) {
    PMAJOR("unknown command flatbuffer");
    return E_INVAL;
  }

  auto msg = GetMessage(in_work_request);

  /*----------------------*/
  /* put request handling */
  /*----------------------*/
  auto put_request = msg->command_as_PutRequest();
  if(put_request) {
if(1) {	  
    auto str = put_request->row()->c_str();

  const char *p;  


// STX
//
#ifdef DEBUG_PLUGIN
   std::cout << "devide fields " << std::endl;
#endif   
   std::vector<string> fields;
   boost::split( fields, str, is_any_of(" ") );
   uint64_t num0 = lexical_cast<uint64_t>(fields[0]);

   const char * c = fields[1].c_str();
   uint64_t num1 = char8B_to_uint64(c);

   std::stringstream ss;
   uint64_t num2;
   ss << std::hex << fields[2];
   ss >> num2;
   fields[3].erase(fields[3].length()-1);
   uint64_t num3 = lexical_cast<uint64_t>(fields[3]);
#ifdef DEBUG_PLUGIN
   std::cout << "num0 " << num0 << std::endl; 
   std::cout << "num1 " << num1 << "  felids1 " << fields[1].c_str() <<  std::endl; 
   std::cout << "num2 " << num2 << "  felids2 " << fields[2].c_str() << std::endl; 
   std::cout << "num3 " << num3 << std::endl; 
#endif

#ifdef ELASTIC 
   UUID obj_time;
   obj_time.val[0] = num0; // time
   obj_time.val[1] = num2; // obj
   table[row_nu] = {obj_time, num1, num3};
   idx1->Insert(table[row_nu].obj_time, &table[row_nu].obj_time);
#endif   
#ifdef HOT
   UUID obj_time;
   obj_time.val[0] = __builtin_bswap64(num2);  
   obj_time.val[1] = __builtin_bswap64(num0); 
   table[row_nu] = {obj_time, num1, num3};
   hot1.insert(&(table[row_nu].obj_time)); 
#endif


}

   row_nu++;
   return S_OK;
  }

  /*------------------------------*/
  /* build index request handling */
  /*------------------------------*/
  if(msg->command_as_BuildIndex()) {
	  
#ifdef ELASTIC
	 std::cout << "sizeof(table) " << sizeof(table)/1024.0/1024.0 <<" [MB]" << std::endl;
	 idx1->PrintTreeStats();
#endif
#ifdef HOT
	 std::pair<size_t, std::map<std::string, double>> hot_stats;
	 hot_stats = hot1.getStatistics(); 
	 std::cout <<"hot total size " << hot_stats.first/1024.0/1024.0  << " [MB] " << std::endl;
#endif	 
         return S_OK;
  }

  /*---------------------------------------*/
  /* requesting for given string    */
  /*---------------------------------------*/  
  auto get_symbol_request = msg->command_as_GetSymbol();
  uint64_t cnter = 0;
  if(get_symbol_request) {// seatch query
if(1){ 
     	auto req_str = get_symbol_request->row()->c_str();
//    auto& req_row = *(get_symbol_request->row());
#ifdef DEBUG_PLUGIN
    std::cout << "req_str " << req_str << std::endl;
    std::cout << "devide fields " << std::endl;
#endif
    std::vector<string> fields;
    boost::split( fields, req_str, is_any_of(" ") );
   std::stringstream ss;
   uint64_t obj;
   ss << std::hex << fields[0];
   ss >> obj;
   uint64_t stime = lexical_cast<uint64_t>(fields[1]);
   uint64_t isrange = lexical_cast<uint64_t>(fields[2]);

   KeyType req;
#ifdef ELASTIC    
   req.val[1]  =  obj;
   req.val[0] = stime;
#endif
#ifdef HOT    
   req.val[1]  =  __builtin_bswap64(stime);
   req.val[0] = __builtin_bswap64(obj);
#endif



   if (isrange == 0) {
#ifdef DEBUG_PLUGIN
	   std::cout << " after split " << obj  << " " << stime << " " << jump << std::endl;
#endif
#ifdef ELASTIC    
	   //    cnter = idx1->GetRangeTime(req, v2, jump);
	   cnter = idx1->GetExact(req);
    
#endif
#ifdef HOT    
	   req.val[1]  =  __builtin_bswap64(obj);
	   req.val[0] = __builtin_bswap64(stime);
	   auto res = hot1.lookup(req);

	   if (res.mIsValid) {
		   //	   ValueType key = (ValueType) res.mValue;
		   //	   printf ("ptr %ld key[0] %ld, key[1] %ld\n", key, key->val[0], key->val[1]);
		   cnter = 1;
	   }
#endif   

#ifdef DEBUG_PLUGIN
	   std::cout << " the COUNTER symbol in this range is "<< res0 << std::endl;
#endif
   }
   else { ////////////////////// RANGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
          ////////////////////// RANGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
           ////////////////////// RANGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifdef ELASTIC
	   cnter = idx1->GetRange(req);
#endif	   
#ifdef HOT
	   /// NOT handle arrive to the end of the index 	
	   hot::singlethreaded::HOTSingleThreaded<ValueType, KeyExtractor>::const_iterator res = hot1.lower_bound(req);
	   std::cout << " line 557 " << std::endl; 
	   ValueType key = (ValueType) *res;
	   std::cout << " line 559 " << std::endl; 
	   uint64_t sum = __builtin_bswap64(key->val[1]) ;
	   printf ("res %lu &key[0] %lu key[0] %lu, key[1] %lu\n", res,  &key->val[0], __builtin_bswap64(key->val[0]), __builtin_bswap64(key->val[1]));
	   std::cout << " line 562 " << std::endl; 
	   for (int i = 1; i < RANGE_NUM ; i++) {
	   	++res;
		key = (ValueType) *res;
		std::cout << " line 564 " << std::endl; 
		if(key == 0) {
			printf ("key ++ %lu and  i=%lu\n", key , i);
			break;
		}
		printf ("i %lu res %lu &key[0] %lu key[0] %lu, key[1] %lu\n", i, res, &key->val[0],  __builtin_bswap64(key->val[0]), __builtin_bswap64(key->val[1]));
		sum +=  __builtin_bswap64(key->val[1]);
	   }
	   cnter = sum;
#endif   

   }
  }
    auto result = new uint64_t;
    *result = reinterpret_cast<uint64_t>(cnter);
    response_buffers.emplace_back(result, sizeof(uint64_t), response_buffer_t::alloc_type_malloc{});
    return S_OK;
  }

  /*---------------------------------------*/
  /* requesting string for provided sym id */
  ///*---------------------------------------*/
  auto get_string_request = msg->command_as_GetString();
  if(get_string_request) { 
    return S_OK;
  }

  PERR("unhandled command");
  return E_INVAL;
}
status_t ADO_rangeindex_plugin::shutdown() {
  /* here you would put graceful shutdown code if any */
  return S_OK;
}

/**
 * Factory-less entry point
 *
 */
extern "C" void *factory_createInstance(component::uuid_t interface_iid) {
  PLOG("instantiating cpp-rangeindex-plugin");
  if (interface_iid == interface::ado_plugin)
    return static_cast<void *>(new ADO_rangeindex_plugin());
  else
    return NULL;
}

#undef RESET_STATE

