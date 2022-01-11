/*
 * Description  : PM sorting ADO
 * Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
 * Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
 * License      : Apache License, Version 2.0
 * Done as part of the Workshop on Search Data Structures for High-Performance Databases
 * at Tel-Aviv University.
 */

#include <libpmem.h>
#include <common/logging.h>
#include <common/utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <api/interfaces.h>
#include <string.h>
#include <map>
#include <stdio.h>
#include <sys/mman.h>
#include "cpp_pm_sort_plugin.h"

#include <flatbuffers/flatbuffers.h>
#include <cpp_pm_sort_proto_generated.h>

using namespace flatbuffers;
using namespace cpp_pm_sort_protocol;

#define SUPERBLOCK_CHUNK ("superblock")

static uint64_t sort_start;
static uint64_t quicksort_done;
static uint64_t merge_done;

status_t ADO_cpp_pm_sort_plugin::register_mapped_memory(void * shard_vaddr,
							void * local_vaddr,
							size_t len)
{
	size_t num_of_chunks = len / PM_SORT_CHUNK_SIZE;

	PLOG("ADO_cpp_pm_sort_plugin: register_mapped_memory (%p, %p, %lu, %lu)",
			 shard_vaddr, local_vaddr, len, num_of_chunks);
	assert(num_of_chunks);

	return S_OK;
}

inline void * copy_flat_buffer(FlatBufferBuilder& fbb)
{
	auto fb_len = fbb.GetSize();
	void * ptr = ::malloc(fb_len);
	memcpy(ptr, fbb.GetBufferPointer(), fb_len);
	return ptr;
}


uint64_t now_ms() {
	return std::chrono::time_point_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now()).time_since_epoch().count();
}

void ADO_cpp_pm_sort_plugin::update_superblock(ADO_cpp_pm_sort_plugin_superblock *sb,
					       enum pm_sorting_state_t state,
					       uint64_t last_modified_index = (PM_SORT_RECORD_ARRAY_SIZE(0)*PM_SORT_NUMBER_OF_CHUNKS)+2,
					       uint64_t merge_phase = 0){
	for (uint8_t k = 0; k < PM_SORT_NR_SUPERBLOCKS; k++) {
		sb[k].valid = false;
		pmem_persist(sb, sizeof(*sb));
		sb[k].timestamp = time(NULL);
		sb[k].state =state;
		if (last_modified_index != (PM_SORT_RECORD_ARRAY_SIZE(0)*PM_SORT_NUMBER_OF_CHUNKS)+2)
			sb[k].last_modified_index = last_modified_index;
		if (merge_phase != 0)
			sb[k].merge_phase = merge_phase;
		pmem_persist(sb, sizeof(*sb));
		sb[k].valid = true;
		pmem_persist(sb, sizeof(*sb));
	}
}

void ADO_cpp_pm_sort_plugin::swap(ADO_cpp_pm_sort_plugin_superblock *sb,
				  ADO_cpp_pm_sort_plugin_record *records,
				  int i,
				  int j)
{

	ADO_cpp_pm_sort_plugin_record tmp{};
	memcpy(&tmp, &records[i], PM_SORT_RECORD_SIZE);
	memcpy(&records[i], &records[j], PM_SORT_RECORD_SIZE);
	memcpy(&records[j], &tmp, PM_SORT_RECORD_SIZE);
}

// Compare two records and return true iff rec1 < rec2
int ADO_cpp_pm_sort_plugin::is_smaller(ADO_cpp_pm_sort_plugin_record *rec1,
				       ADO_cpp_pm_sort_plugin_record *rec2)
{
	int n = 10; // size of key
	auto *p1 = (uint8_t *) rec1;
	auto *p2 = (uint8_t *) rec2;

	for (int i = 0; i < n - 1; i++) {
		if (*p1 == *p2) {
			p1++; p2++;
		}
	}

	return *p1 < *p2;
}

int ADO_cpp_pm_sort_plugin::max_record(ADO_cpp_pm_sort_plugin_record *chunk, int a, int b)
{
	if (is_smaller(&chunk[a], &chunk[b]))
		return b;
	return a;
}

int ADO_cpp_pm_sort_plugin::min_record(ADO_cpp_pm_sort_plugin_record *chunk, int a, int b)
{
	if (is_smaller(&chunk[a], &chunk[b]))
		return a;
	return b;
}

// Find pivot based on median of 3
int ADO_cpp_pm_sort_plugin::get_pivot(ADO_cpp_pm_sort_plugin_record *chunk,
					 int start,
					 int end)
{
	int middle = start + (end - start) / 2;
	return max_record(chunk, min_record(chunk, start, middle), min_record(chunk, max_record(chunk, start, middle), end));
}



/* place the pivot at its correct position in sorted
array. and place all records smaller than pivot to left of pivot and all greater to right of pivot */
int ADO_cpp_pm_sort_plugin::partition(ADO_cpp_pm_sort_plugin_superblock *sb,
				      ADO_cpp_pm_sort_plugin_record *chunk,
				      int start,
				      int end)
{
	int pivot_ind = get_pivot(chunk, start, end);
	swap(sb, chunk, pivot_ind, end);
	ADO_cpp_pm_sort_plugin_record *pivot = &chunk[end];
	int i = start - 1;
	for (int j = start; j < end; j++)
	{
		if (is_smaller(&chunk[j], pivot))
		{
			i++;
			swap(sb, chunk, i, j);
		}
	}
	swap(sb, chunk, i + 1, end);
	return i + 1;
}

void ADO_cpp_pm_sort_plugin::quicksort(ADO_cpp_pm_sort_plugin_superblock *sb,
				       ADO_cpp_pm_sort_plugin_record *chunk,
				       int start,
				       int end)
{
	if (start < end)
	{
		/* p is partitioning index, arr[p] is now at right place */
		int p = partition(sb, chunk, start, end);
		// Sort records before partition and after partition
		quicksort(sb, chunk, start, p - 1);
		quicksort(sb, chunk, p + 1, end);
	}
}

// Sort one chunk
void ADO_cpp_pm_sort_plugin::create_sorted_chunk(ADO_cpp_pm_sort_plugin_superblock *sb,
						 ADO_cpp_pm_sort_plugin_record *chunk,
						 const std::string& chunk_name,
						 const uint64_t work_key,
						 int task,
						 std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	// Create key for sorted chunk
	ADO_cpp_pm_sort_plugin_record *sorted_chunk;
	void * out_value_addr = nullptr;
	const char *new_key_addr = nullptr;
	component::IKVStore::key_t key_handle;
	std::string sorted_chunk_name =  chunk_name + "Sorted";

	if (task == 3) {
		if (cb_create_key(work_key,
				  sorted_chunk_name,
				  PM_SORT_CHUNK_SIZE,
				  FLAGS_NO_IMPLICIT_UNLOCK,
				  out_value_addr,
				  &new_key_addr,
				  &key_handle) != S_OK)
			throw General_exception("failed to allocate new chunk");

		if(cb_unlock(work_key, key_handle) != S_OK)
			throw General_exception("unlocking key");
		sorted_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(out_value_addr);
		// create sorted copy of chunk i
		memcpy(sorted_chunk, chunk, PM_SORT_CHUNK_SIZE);
	}

	else/*if ((task == 1) || (task == 2))*/ {
		sorted_chunk = (*data_map)[chunk_name];
	}
	//sort the chunk
	quicksort(sb, sorted_chunk, 0, PM_SORT_RECORD_ARRAY_SIZE(0) - 1);

	if (task == 3) {
		pmem_persist(sorted_chunk, PM_SORT_CHUNK_SIZE);
	}
	else /*if ((task == 1) || (task == 2))*/ {
		data_map->insert(std::pair<std::string, ADO_cpp_pm_sort_plugin_record *>
		(sorted_chunk_name, sorted_chunk));
		data_map->erase(chunk_name);
	}
}

#define CHUNKS_TO_RECORD(chunks, index) \
	(chunks[(index)/PM_SORT_RECORD_ARRAY_SIZE(0)][(index)%PM_SORT_RECORD_ARRAY_SIZE(0)])

bool ADO_cpp_pm_sort_plugin::verify(const uint64_t work_key)
{
	uint64_t chunk_type = get_final_merge_phase() + 1;
	component::IKVStore::key_t key_handles[PM_SORT_NUMBER_OF_CHUNKS];
	ADO_cpp_pm_sort_plugin_record *chunks[PM_SORT_NUMBER_OF_CHUNKS];
	bool res = false;
	size_t value_size = 0;
	//read all chunks from PM
	for (uint64_t i = 0; i < PM_SORT_NUMBER_OF_CHUNKS; i++) {
		void *out_value_addr;
		auto key = "L"+ std::to_string(chunk_type) +"chunk" + std::to_string(i);
		if(cb_open_key(work_key,
			       key,
			       FLAGS_NO_IMPLICIT_UNLOCK,
			       out_value_addr,
			       value_size,
			       nullptr,
			       &key_handles[i]) != S_OK) {
			throw General_exception("opening key in verify");
		}

		if (value_size != PM_SORT_CHUNK_SIZE) {
			throw General_exception("wrong chunk size %lu", value_size);
		}

		chunks[i] = static_cast<ADO_cpp_pm_sort_plugin_record *>(out_value_addr);
	}
	//check records are sorted across all the chunks
	for (uint64_t i = 0; i < PM_SORT_NUMBER_OF_CHUNKS*PM_SORT_RECORD_ARRAY_SIZE(0) - 1; i++) {
		if (is_smaller(&CHUNKS_TO_RECORD(chunks, i + 1), &CHUNKS_TO_RECORD(chunks, i))) {
			goto out;
		}
	}

	res = true;
out:
	for (uint64_t i = 0; i < PM_SORT_NUMBER_OF_CHUNKS; i++) {
		if(cb_unlock(work_key, key_handles[i]) != S_OK)
			throw General_exception("unlocking key");
	}

	return res;
}

uint64_t ADO_cpp_pm_sort_plugin::get_final_merge_phase(){
	uint64_t final_merge_phase = 0;
	uint64_t num_of_chunks = PM_SORT_NUMBER_OF_CHUNKS;
	while (num_of_chunks > 1){
		final_merge_phase++;
		num_of_chunks = num_of_chunks>>1;
	}
	return final_merge_phase;
}

void ADO_cpp_pm_sort_plugin::chunk_merge(ADO_cpp_pm_sort_plugin_record *dst_chunk,
				   ADO_cpp_pm_sort_plugin_record *src_chunk_1,
				   ADO_cpp_pm_sort_plugin_record *src_chunk_2,
				   uint64_t merge_phase)
{
	uint64_t j = 0;
	uint64_t k = 0;
	uint64_t src_array_size = PM_SORT_RECORD_ARRAY_SIZE(merge_phase - 1);
	uint64_t dst_array_size = PM_SORT_RECORD_ARRAY_SIZE(merge_phase);

	for (uint64_t i = 0; i < dst_array_size; i++) {
		if (k == src_array_size) {
			while (j <  src_array_size){
				memcpy(&dst_chunk[i], &src_chunk_1[j], PM_SORT_RECORD_SIZE);
				j++;
				i++;
			}
			return;
		}
		if (j == src_array_size) {
			while (k <  src_array_size){
				memcpy(&dst_chunk[i], &src_chunk_2[k], PM_SORT_RECORD_SIZE);
				k++;
				i++;
			}
			return;
		}
		if (is_smaller(&src_chunk_1[j], &src_chunk_2[k])) {
			memcpy(&dst_chunk[i], &src_chunk_1[j], PM_SORT_RECORD_SIZE);
			j++;
		} else {
			memcpy(&dst_chunk[i], &src_chunk_2[k], PM_SORT_RECORD_SIZE);
			k++;
		}
	}
	return;
}
/*returns null if no valid superblock exists in sb. should never happen*/
ADO_cpp_pm_sort_plugin_superblock * ADO_cpp_pm_sort_plugin::get_updated_superblock(ADO_cpp_pm_sort_plugin_superblock *sb)
{
	ADO_cpp_pm_sort_plugin_superblock * updated_sb = nullptr;
	for (uint32_t k = 0; k < PM_SORT_NR_SUPERBLOCKS; k++) {
		if(sb[k].valid && (!updated_sb || sb[k].timestamp > updated_sb->timestamp))
			updated_sb = &sb[k];
	}
	return updated_sb;
}

status_t ADO_cpp_pm_sort_plugin::sort_chunks(ADO_cpp_pm_sort_plugin_superblock *sb,
					     const uint64_t work_key,
					     uint64_t chunk_index,
					     int task,
					     std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	void * out_value_addr = nullptr;
	size_t value_size = 0;
	component::IKVStore::key_t key_handle;
	ADO_cpp_pm_sort_plugin_record * curr_chunk;
	std::string chunk_name;

	// quicksort each chunk starting from last modified curr_chunk
	for (uint64_t i = chunk_index; i < PM_SORT_NUMBER_OF_CHUNKS; i++) {
		PLOG("Sorting chunk: %lu", i);
		chunk_name =  "L0chunk" + std::to_string(i);

		if (task == 3) {
			update_superblock(sb,PM_SORT_CHUNK,i);
			if(cb_open_key(work_key,
				       chunk_name,
				       FLAGS_NO_IMPLICIT_UNLOCK,
				       out_value_addr,
				       value_size,
				       nullptr,
				       &key_handle) != S_OK)
				throw General_exception("opening existing key");

			curr_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(out_value_addr);
		}

		else /*if ((task == 1) || (task == 2))*/ {
			curr_chunk = (*data_map)[chunk_name];
		}

		create_sorted_chunk(sb, curr_chunk, chunk_name, work_key, task, data_map);

		if (task == 3) {
			if(cb_unlock(work_key, key_handle) != S_OK)
				throw General_exception("unlocking key");
			if(cb_erase_key( "L0chunk" + std::to_string(i)) != S_OK)
				throw General_exception("erasing existing key");
		}
	}
	return S_OK;
}

status_t ADO_cpp_pm_sort_plugin::merge(ADO_cpp_pm_sort_plugin_superblock *sb,
				       const uint64_t work_key,
				       uint64_t chunk_index,
				       uint64_t start_merge_phase,
				       int task,
				       std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	const char *new_key_addr = nullptr;
	status_t rc = S_OK;
	std::string err;
	uint64_t merge_phase = 1;
	uint64_t src_chunk_size = PM_SORT_CHUNK_SIZE;
	uint64_t dst_chunk_size = PM_SORT_CHUNK_SIZE<<1;
	uint64_t chunks_in_phase = PM_SORT_NUMBER_OF_CHUNKS;
	component::IKVStore::key_t dst_key_handle;
	component::IKVStore::key_t src_1_key_handle;
	component::IKVStore::key_t src_2_key_handle;
	std::string dst_chunk_name;
	std::string src_chunk_1_name;
	std::string src_chunk_2_name;
	void *merge_dst_value_addr = nullptr;
	void *merge_src_1_value_addr = nullptr;
	void *merge_src_2_value_addr = nullptr;
	ADO_cpp_pm_sort_plugin_record *dst_chunk;
	ADO_cpp_pm_sort_plugin_record *src_chunk_1;
	ADO_cpp_pm_sort_plugin_record *src_chunk_2;

	while (chunks_in_phase > 1){
		if (merge_phase >= start_merge_phase) {
			if (task == 3){
				update_superblock(sb,PM_SORT_MERGE,chunk_index,merge_phase);
			}
			else if (task == 2){
				update_superblock(sb,PM_SORT_MERGE,0,merge_phase);
				/*no need to stabilize on first iteration, merge called when stable*/
				if (merge_phase > start_merge_phase)
					stabilize_PM(work_key,(int)(merge_phase-1));
			}

			PLOG("starting merge phase: %lu", merge_phase);
			while(chunk_index < chunks_in_phase) {
				if (task == 3)
					update_superblock(sb,PM_SORT_MERGE,chunk_index,merge_phase);

				/*open a destination chunk and two chunks to merge into it*/
				dst_chunk_name =
					"L" + std::to_string(merge_phase) + "chunk" + std::to_string(chunk_index >> 1);
				if (task == 3) {
					rc = cb_create_key(work_key,
							   dst_chunk_name,
							   dst_chunk_size,
							   FLAGS_NO_IMPLICIT_UNLOCK,
							   merge_dst_value_addr,
							   &new_key_addr,
							   &dst_key_handle);
					if (rc != S_OK) {
						err = "failed to allocate " + dst_chunk_name;
						throw General_exception(err.c_str());
					}

					dst_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(merge_dst_value_addr);
				}

				else /*if ((task == 1) || (task == 2))*/ {
					dst_chunk = (ADO_cpp_pm_sort_plugin_record *) malloc(dst_chunk_size);
					data_map->insert(std::pair<std::string, ADO_cpp_pm_sort_plugin_record *>
					(dst_chunk_name, dst_chunk));
				}


				src_chunk_1_name =
					"L" + std::to_string(merge_phase - 1) + "chunk" + std::to_string(chunk_index);
				if (merge_phase == 1) src_chunk_1_name += "Sorted";

				if (task == 3) {
					rc = cb_open_key(work_key,
							 src_chunk_1_name,
							 FLAGS_NO_IMPLICIT_UNLOCK,
							 merge_src_1_value_addr,
							 src_chunk_size,
							 &new_key_addr,
							 &src_1_key_handle);
					if (rc != S_OK) {
						err = "failed to open " + src_chunk_1_name;
						throw General_exception(err.c_str());
					}
					src_chunk_1 = static_cast<ADO_cpp_pm_sort_plugin_record *>(merge_src_1_value_addr);
				}

				else /*if ((task == 1) || (task == 2))*/ {
					src_chunk_1 = (*data_map)[src_chunk_1_name];
				}


				src_chunk_2_name = "L" + std::to_string(merge_phase - 1) + "chunk" +
						   std::to_string(chunk_index + 1);
				if (merge_phase == 1) src_chunk_2_name += "Sorted";

				if (task == 3) {
					rc = cb_open_key(work_key,
							 src_chunk_2_name,
							 FLAGS_NO_IMPLICIT_UNLOCK,
							 merge_src_2_value_addr,
							 src_chunk_size,
							 &new_key_addr,
							 &src_2_key_handle);
					if (rc != S_OK) {
						err = "failed to open " + src_chunk_2_name;
						throw General_exception(err.c_str());
					}
					src_chunk_2 = static_cast<ADO_cpp_pm_sort_plugin_record *>(merge_src_2_value_addr);
				}

				else /*if ((task == 1) || (task == 2))*/ {
					src_chunk_2 = (*data_map)[src_chunk_2_name];
				}


				chunk_merge(dst_chunk, src_chunk_1, src_chunk_2, merge_phase);

				if (task == 3) {
					pmem_persist(dst_chunk, dst_chunk_size);
					// Unlock before erase to free memory
					if (cb_unlock(work_key, src_1_key_handle) != S_OK)
						throw General_exception("unlocking key");
					if (cb_unlock(work_key, src_2_key_handle) != S_OK)
						throw General_exception("unlocking key");

					if (cb_erase_key(src_chunk_1_name) != S_OK)
						throw General_exception("erasing key");
					if (cb_erase_key(src_chunk_2_name) != S_OK)
						throw General_exception("erasing key");

					if(cb_unlock(work_key, dst_key_handle) != S_OK)
						throw General_exception("unlocking key");
				}

				else /*if ((task == 1) || (task == 2))*/ {
					free(src_chunk_1);
					free(src_chunk_2);
					data_map->erase(src_chunk_1_name);
					data_map->erase(src_chunk_2_name);
				}

				chunk_index += 2;
			}
			chunk_index = 0;
			if(task == 2)
				backup_to_PM(work_key,merge_phase,data_map);
		}
		chunks_in_phase = chunks_in_phase>>1;
		merge_phase++;
		src_chunk_size = src_chunk_size*2;
		dst_chunk_size = dst_chunk_size*2;
	}
	return S_OK;
}

status_t ADO_cpp_pm_sort_plugin::split_result(ADO_cpp_pm_sort_plugin_superblock *sb,
					      const uint64_t work_key,
					      uint64_t chunk_index,
					      int task,
					      std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	/*open the merged chunk containing all DB, to partition it*/
	status_t rc = S_OK;
	std::string err;
	const char *new_key_addr = nullptr;
	component::IKVStore::key_t output_key_handle;
	component::IKVStore::key_t merged_key_handle;
	uint64_t DB_size = PM_SORT_CHUNK_SIZE*PM_SORT_NUMBER_OF_CHUNKS;
	std::string output_chunk_name;
	/*compute how many merge phases happened for appropriate name for final result*/
	uint64_t final_merge_phase = get_final_merge_phase();

	std::string merged_chunk_name = "L" + std::to_string(final_merge_phase) + "chunk0";
	void *output_value_addr = nullptr;
	void *merged_value_addr = nullptr;
	ADO_cpp_pm_sort_plugin_record *output_chunk;
	ADO_cpp_pm_sort_plugin_record *merged_chunk;
	PLOG("splitting result into chunks");

	if (task == 3) {
		rc = cb_open_key(work_key,
				 merged_chunk_name,
				 FLAGS_NO_IMPLICIT_UNLOCK,
				 merged_value_addr,
				 DB_size,
				 &new_key_addr,
				 &merged_key_handle);
		if(rc != S_OK) {
			err = "failed to open " + merged_chunk_name;
			throw General_exception(err.c_str());
		}
		merged_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(merged_value_addr);
	}

	else /*if ((task == 1) || (task == 2))*/ {
		merged_chunk = (*data_map)[merged_chunk_name];
	}

	for(uint64_t i = chunk_index; i < PM_SORT_NUMBER_OF_CHUNKS; i++) {

		output_chunk_name = "L" + std::to_string(final_merge_phase+1) + "chunk" +  std::to_string(i);
		if (task == 3) {
			update_superblock(sb,PM_SORT_SPLIT_RESULT,i);
			rc = cb_create_key(work_key,
					   output_chunk_name,
					   PM_SORT_CHUNK_SIZE,
					   FLAGS_NO_IMPLICIT_UNLOCK,
					   output_value_addr,
					   &new_key_addr,
					   &output_key_handle);
			output_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(output_value_addr);
		}

		else /*if ((task == 1) || (task == 2))*/ {
			output_chunk = (ADO_cpp_pm_sort_plugin_record *) malloc(PM_SORT_CHUNK_SIZE);
			data_map->insert(std::pair<std::string, ADO_cpp_pm_sort_plugin_record *>
			(output_chunk_name, output_chunk));
		}

		pmem_memcpy_persist(output_chunk, &merged_chunk[(i * PM_SORT_RECORD_ARRAY_SIZE(0))], PM_SORT_CHUNK_SIZE);

		if (task == 3) {
			if (cb_unlock(work_key, output_key_handle) != S_OK)
				throw General_exception("unlocking key");
		}
	}
 	if (task == 3) {
 		// Unlock before erase to free memory
 		if(cb_unlock(work_key, merged_key_handle) != S_OK)
 			throw General_exception("unlocking key");
 		if(cb_erase_key(merged_chunk_name) != S_OK)
 			throw General_exception("erasing key");
 	}

 	else /*if ((task == 1) || (task == 2))*/{
 		free(merged_chunk);
 		data_map->erase(merged_chunk_name);
 	}

	return S_OK;
}

status_t ADO_cpp_pm_sort_plugin::sort_task3(ADO_cpp_pm_sort_plugin_superblock *sb, const uint64_t work_key)
{
	void * out_value_addr = nullptr;
	void * out_value_addr2 = nullptr;
	size_t value_size = 0;
	component::IKVStore::key_t key_handle;
	component::IKVStore::key_t key_handle2;
	status_t rc1 = S_OK;
	status_t rc2 = S_OK;
	std::string key_name1;
	std::string key_name2;
	bool index_flag = false;

	sort_start = now_ms();

	// Restore state
	bool quicksort_needed = false;
	bool merge_needed = false;
	uint64_t chunk_index = 0;
	uint64_t merge_phase = 1;
	ADO_cpp_pm_sort_plugin_superblock * updated_sb = get_updated_superblock(sb);

	if (updated_sb->state == PM_SORT_IDLE) {
		// Start sorting from scratch
		quicksort_needed = true;
		merge_needed = true;
	}

	else if (updated_sb->state == PM_SORT_CHUNK) {
		// Start quicksort after last complete curr_chunk
		quicksort_needed = true;
		merge_needed = true;
		chunk_index = updated_sb->last_modified_index;
		/* if unsorted chunk at chunk_index does not exist, it means crash happened after sorting it,
		 * before updating the superblock. thus, you can move to the next index*/
		key_name1 = "L0chunk" + std::to_string(chunk_index);
		if(cb_open_key(work_key,
			       key_name1,
			       FLAGS_NO_IMPLICIT_UNLOCK,
			       out_value_addr,
			       value_size,
			       nullptr,
			       &key_handle) != S_OK){
			chunk_index = chunk_index + 1;
			update_superblock(sb,PM_SORT_MERGE,chunk_index);
		}
		else{
			if (cb_unlock(work_key, key_handle) != S_OK)
				throw General_exception("unlocking key");
			/*delete key-value pair for sorted version of chunk if it was created (it might contain garbage)*/
			key_name1 = "L0chunk" + std::to_string(chunk_index) + "Sorted";
			if(cb_open_key(work_key,
				       key_name1,
				       FLAGS_NO_IMPLICIT_UNLOCK,
				       out_value_addr,
				       value_size,
				       nullptr,
				       &key_handle) == S_OK){
				if (cb_unlock(work_key, key_handle) != S_OK)
					throw General_exception("unlocking key");
				if (cb_erase_key(key_name1) != S_OK)
					throw General_exception("erasing key");
			}
		}
	}

	else if (updated_sb->state == PM_SORT_MERGE){
		merge_needed = true;
		chunk_index = updated_sb->last_modified_index;
		merge_phase = updated_sb->merge_phase;
		key_name1 = "L" + std::to_string(merge_phase-1) + "chunk" + std::to_string(chunk_index);
		key_name2 = "L" + std::to_string(merge_phase-1) + "chunk" + std::to_string(chunk_index+1);
		if (merge_phase == 1) {
			key_name1 += "Sorted";
			key_name2 += "Sorted";
		}
		rc1 = cb_open_key(work_key,
				  key_name1,
				  FLAGS_NO_IMPLICIT_UNLOCK,
				  out_value_addr,
				  value_size,
				  nullptr,
				  &key_handle);
		rc2 = cb_open_key(work_key,
				  key_name2,
				  FLAGS_NO_IMPLICIT_UNLOCK,
				  out_value_addr2,
				  value_size,
				  nullptr,
				  &key_handle2);
		/*if one of src chunks was deleted, current merge is done, advance to next index*/
		if(rc1 != S_OK){
			index_flag = true;
			if(rc2 == S_OK){
				if (cb_unlock(work_key, key_handle2) != S_OK)
					throw General_exception("unlocking key");

				if (cb_erase_key(key_name2) != S_OK)
					throw General_exception("erasing key");
			}
		}
		else if(rc2 != S_OK){
			index_flag = true;
			if (cb_unlock(work_key, key_handle) != S_OK)
				throw General_exception("unlocking key");

			if (cb_erase_key(key_name1) != S_OK)
				throw General_exception("erasing key");
		}
		/*otherwise, delete current dst of merge in case it was half-written*/
		else{
			if (cb_unlock(work_key, key_handle) != S_OK)
				throw General_exception("unlocking key");
			if (cb_unlock(work_key, key_handle2) != S_OK)
				throw General_exception("unlocking key");
			key_name1 = "L" + std::to_string(merge_phase) + "chunk" + std::to_string(chunk_index>>1);
			rc1 = cb_open_key(work_key,
					  key_name1,
					  FLAGS_NO_IMPLICIT_UNLOCK,
					  out_value_addr,
					  value_size,
					  nullptr,
					  &key_handle);
			if (rc1 == S_OK){
				if (cb_unlock(work_key, key_handle) != S_OK)
					throw General_exception("unlocking key");

				if (cb_erase_key(key_name1) != S_OK)
					throw General_exception("erasing key");
			}
		}
		if(index_flag){
			chunk_index=chunk_index+2;
			update_superblock(sb,PM_SORT_MERGE,chunk_index,merge_phase);
		}
	}
	else{ //updated_sb->state == PM_SORT_SPLIT_RESULT
		chunk_index = updated_sb->last_modified_index;
		/*compute how many merge phases happened for appropriate name for final result*/
		uint64_t final_merge_phase = get_final_merge_phase();
		key_name1 = "L"+ std::to_string(final_merge_phase +1) +"chunk" + std::to_string(chunk_index);
		/*delete chunk in chunk_index in case it was created but not writen into completely*/
		if(cb_open_key(work_key,
			       key_name1,
			       FLAGS_NO_IMPLICIT_UNLOCK,
			       out_value_addr,
			       value_size,
			       nullptr,
			       &key_handle) == S_OK){
			if (cb_unlock(work_key, key_handle) != S_OK)
				throw General_exception("unlocking key");
			if (cb_erase_key(key_name1) != S_OK)
				throw General_exception("erasing key");
		}
	}

	if (quicksort_needed) {
		sort_chunks(sb,work_key, chunk_index);
		chunk_index = 0;
		merge_phase = 1;
		update_superblock(sb,PM_SORT_MERGE,chunk_index,merge_phase);
		quicksort_done = now_ms();
	}

	if (merge_needed){
		merge(sb,work_key,chunk_index,merge_phase);
		chunk_index = 0;
		update_superblock(sb,PM_SORT_SPLIT_RESULT,chunk_index);
		merge_done = now_ms();
	}

	split_result(sb,work_key,chunk_index);

	// return to idle state
	update_superblock(sb,PM_SORT_IDLE);
	return S_OK;
}

status_t ADO_cpp_pm_sort_plugin::load_data(const uint64_t work_key, ADO_cpp_pm_sort_plugin_superblock *sb)
{
	FILE *chunk_file;
	std::string chunk_name, chunk_path, err;
	component::IKVStore::key_t key_handle;
	status_t rc = S_OK;
	void *out_value_addr = nullptr;
	const char *new_key_addr = nullptr;

	for (uint64_t chunk_num = 0; chunk_num < PM_SORT_NUMBER_OF_CHUNKS; chunk_num++)
	{
		chunk_name = "L0chunk" + std::to_string(chunk_num);
		chunk_path = PM_SORT_CHUNKS_FOLDER_PATH + chunk_name;
		chunk_file = fopen(chunk_path.c_str(), "rb");
		if(!chunk_file) {
			err = "could not open chunk file number " + std::to_string(chunk_num);
			throw General_exception(err.c_str());
		}

		/*create a new key-value pair for the chunk*/
		rc = cb_create_key(work_key,
				   chunk_name,
				   PM_SORT_CHUNK_SIZE,
				   FLAGS_NO_IMPLICIT_UNLOCK,
				   out_value_addr,
				   &new_key_addr,
				   &key_handle);
		if(rc != S_OK) {
			err = "failed to allocate chunk number " + std::to_string(chunk_num);
			throw General_exception(err.c_str());
		}
		PLOG("allocated %s",chunk_name.c_str());
		/*read chunk data from file to PM*/
		if (fread(out_value_addr,
			  PM_SORT_RECORD_SIZE,
			  PM_SORT_RECORD_ARRAY_SIZE(0) ,
			  chunk_file) != PM_SORT_RECORD_ARRAY_SIZE(0)) {
			err = "could not read chunk file number " + std::to_string(chunk_num);
			throw General_exception(err.c_str());
		}

		/*persist*/
		pmem_persist(out_value_addr, PM_SORT_CHUNK_SIZE);
		fclose(chunk_file);
		if(cb_unlock(work_key, key_handle) != S_OK)
			throw General_exception("unlocking key");
	}
	return S_OK;
}


/*Clear all data from pool.*/
status_t ADO_cpp_pm_sort_plugin::clear_pool(const uint64_t work_key)
{
	char *keys[2 * PM_SORT_NUMBER_OF_CHUNKS + 1];
	component::IKVStore::pool_iterator_t  iterator = nullptr;
	component::IKVStore::pool_reference_t r;
	int key_len;
	int i = 0;

	while (cb_iterate(0,0,iterator,r) == S_OK) {
		key_len = int(r.key_len);
		keys[i] = (char *) malloc(key_len + 1);
		keys[i][key_len] = '\0';
		memcpy(keys[i], static_cast<const char *>(r.key), key_len);
		i++;
	}

	for (int j = 0; j < i; j++) {
		if(cb_erase_key(keys[j]) != S_OK)
			throw General_exception("erasing key- clear_pool");
	}

	for (int j = 0; j < i; j++)
		free(keys[j]);

	return S_OK;
}

status_t ADO_cpp_pm_sort_plugin::init(const uint64_t work_key)
{
	std::string chunk_name, chunk_path, err;
	component::IKVStore::key_t sb_key_handle;
	status_t rc = S_OK;
	size_t value_size = 0;
	void *out_value_addr = nullptr;
	const char *new_key_addr = nullptr;
	struct ADO_cpp_pm_sort_plugin_superblock *sb, *updated_sb;

	// Try to read superblock
	rc = cb_open_key(work_key,
			 SUPERBLOCK_CHUNK,
			 FLAGS_NO_IMPLICIT_UNLOCK,
			 out_value_addr,
			 value_size,
			 nullptr,
			 &sb_key_handle);
	if (rc == S_OK) {
		// superblock exists
		PLOG("superblock exists- checking recovery");
		if(cb_unlock(work_key, sb_key_handle))
			throw General_exception("unlocking key");
		sb = static_cast< struct ADO_cpp_pm_sort_plugin_superblock *> (out_value_addr);
		updated_sb = get_updated_superblock(sb);
		if (updated_sb->state != PM_SORT_IDLE) {
			PLOG("recovery detected");
			return S_OK;
		}
		else {
			clear_pool(work_key);
		}
	}

	// create superblock and initialize DB
	if (cb_create_key(work_key,
			  SUPERBLOCK_CHUNK,
			  PM_SORT_SUPERBLOCKS_SIZE,
			  FLAGS_NO_IMPLICIT_UNLOCK,
			  out_value_addr,
			  &new_key_addr,
			  &sb_key_handle) != S_OK)
		throw General_exception("failed to allocate superblocks");

	sb = static_cast< struct ADO_cpp_pm_sort_plugin_superblock *> (out_value_addr);
	update_superblock(sb,PM_SORT_IDLE,0);

	if(cb_unlock(work_key, sb_key_handle) != S_OK)
		throw General_exception("unlocking key");

	load_data(work_key, sb);

	return S_OK;
}


// Copy PM data to Map in RAM. level is the current chunk level saved on PM (0 after init)
// level -1 marks the L0chunkXSorted chunks. works only for levels up to get_final_merge_phase()
status_t ADO_cpp_pm_sort_plugin::load_from_PM(const uint64_t work_key, int level,
						  std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	PLOG("loading from PM to RAM");
	void * out_value_addr = nullptr;
	size_t value_size = 0;
	component::IKVStore::key_t key_handle;
	ADO_cpp_pm_sort_plugin_record * pmem_chunk;
	std::string chunk_name;
	std::string level_identifier;
	ADO_cpp_pm_sort_plugin_record * allocated_chunk;
	bool sorted_required = (level == -1);
	if (sorted_required)
		level_identifier = "L0";
	else
		level_identifier = "L" + std::to_string(level);
	uint64_t chunks_in_level = PM_SORT_NUMBER_OF_CHUNKS;
	uint64_t chunks_size = PM_SORT_CHUNK_SIZE;
	int l = level;
	while (l > 0){
		chunks_in_level = chunks_in_level>>1;
		chunks_size = chunks_size*2;
		l--;
	}


	for (uint64_t i = 0; i < chunks_in_level; i++) {

		chunk_name = level_identifier + "chunk" + std::to_string(i);
		if(sorted_required)
			chunk_name = chunk_name + "Sorted";
		if(cb_open_key(work_key,
			       chunk_name,
			       FLAGS_NO_IMPLICIT_UNLOCK,
			       out_value_addr,
			       value_size,
			       nullptr,
			       &key_handle) != S_OK)
			throw General_exception("opening existing chunk");

		pmem_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(out_value_addr);
		allocated_chunk = (ADO_cpp_pm_sort_plugin_record *) malloc(chunks_size);
		memcpy(allocated_chunk, pmem_chunk, chunks_size);
		data_map->insert(std::pair<std::string, ADO_cpp_pm_sort_plugin_record *>(chunk_name, allocated_chunk));

		if(cb_unlock(work_key, key_handle) != S_OK)
			throw General_exception("unlocking chunk");
	}
	return S_OK;
}


status_t ADO_cpp_pm_sort_plugin::free_map(std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	ADO_cpp_pm_sort_plugin_record *chunk_value;
	std::string chunk_name;

	auto it = data_map->begin();
	while (it != data_map->end()) {
		chunk_name = it->first;
		chunk_value = it->second;
		free(chunk_value);
		it++;
	}
	data_map->clear();
	return S_OK;
}

// sort on RAM
status_t ADO_cpp_pm_sort_plugin::sort_task1(const uint64_t work_key)
{
	std::map<std::string, ADO_cpp_pm_sort_plugin_record*> data_map;

	sort_start = now_ms();
	load_from_PM(work_key, 0, &data_map);

	// quicksort chunks on RAM
	sort_chunks(nullptr, work_key, 0, 1, &data_map);

	quicksort_done = now_ms();

	// merge on RAM
	merge(nullptr, work_key, 0, 1, 1, &data_map);

	merge_done = now_ms();

	// split result on RAM
	split_result(nullptr, work_key, 0, 1, &data_map);

	// write back to PM
	backup_to_PM(work_key, 0, &data_map);

	//free chunks in map
	free_map(&data_map);
	return S_OK;
}

/* for task2. ensure PM only contains the relevant most recent backup (chunks of level "level")
 * level -1 marks "L0chunkXSorted" chunks */
status_t ADO_cpp_pm_sort_plugin::stabilize_PM(const uint64_t work_key, int level)
{
	char *keys[PM_SORT_NUMBER_OF_CHUNKS*2 + 1]; //maximal number of concurrent keys in PM
	component::IKVStore::pool_iterator_t  iterator = nullptr;
	component::IKVStore::pool_reference_t r;
	std::string level_identifier;
	std::string key_str;
	int key_len;
	bool is_sorted_chunk_key;
	bool is_superblock;
	bool different_level;
	bool sorted_required = (level == -1);
	if (sorted_required)
		level_identifier = "L0";
	else
		level_identifier = "L" + std::to_string(level);


	int i = 0;
	while (cb_iterate(0,0,iterator,r) == S_OK) {
		key_len = int(r.key_len);
		keys[i] = (char *) malloc(key_len + 1);
		keys[i][key_len] = '\0';
		memcpy(keys[i], static_cast<const char *>(r.key), key_len);
		i++;
	}

	for (int j = 0; j < i; j++) {
		key_str = keys[j];
		different_level = (key_str.find(level_identifier) == std::string::npos);
		is_sorted_chunk_key = (key_str.find("Sorted") != std::string::npos);
                is_superblock = (key_str.find(SUPERBLOCK_CHUNK) != std::string::npos);
		if ((!is_superblock)&&(different_level || (is_sorted_chunk_key != sorted_required))) {
                        if(cb_erase_key(key_str) != S_OK)
                                throw General_exception("erasing key- stabilize_PM");
		}
	}

	for (int j = 0; j < i; j++)
		free(keys[j]);

	return S_OK;
}

/*copies all chunks in map to PM*/
status_t ADO_cpp_pm_sort_plugin::backup_to_PM(const uint64_t work_key, uint64_t level,
						  std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	PLOG("backing up RAM to PM");
	ADO_cpp_pm_sort_plugin_record *chunk_value;
	std::string chunk_name;
	void * out_value_addr = nullptr;
	const char *new_key_addr = nullptr;
	component::IKVStore::key_t key_handle;
	uint64_t chunks_size = PM_SORT_CHUNK_SIZE;
	if (level == get_final_merge_phase() + 1)
		level = 0;
	while (level > 0){
		chunks_size = chunks_size*2;
		level--;
	}

	auto it = data_map->begin();
	while (it != data_map->end()) {
		chunk_name = it->first;
		chunk_value = it->second;
		// create new key with sorted data
		if (cb_create_key(work_key,
				  chunk_name,
				  chunks_size,
				  FLAGS_NO_IMPLICIT_UNLOCK,
				  out_value_addr,
				  &new_key_addr,
				  &key_handle) != S_OK)
			throw General_exception("failed to allocate new chunk");
		pmem_memcpy_persist(out_value_addr, chunk_value, chunks_size);
		if(cb_unlock(work_key, key_handle) != S_OK)
			throw General_exception("unlocking key");
		it++;
	}
	return S_OK;
}

/*task2 specific function, writes final result to PM from map and updates superblock*/
status_t ADO_cpp_pm_sort_plugin::backup_final_result(ADO_cpp_pm_sort_plugin_superblock *sb,
						     const uint64_t work_key,
						     uint64_t chunk_index,
						     std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map)
{
	PLOG("writing final result from RAM to PM");
	std::string chunk_name;
	void *value_addr = nullptr;
	const char *key_addr = nullptr;
	component::IKVStore::key_t key_handle;

	uint64_t final_merge_phase = get_final_merge_phase();
	ADO_cpp_pm_sort_plugin_record *PM_chunk;
	ADO_cpp_pm_sort_plugin_record *RAM_chunk;

	for(uint64_t i = chunk_index; i < PM_SORT_NUMBER_OF_CHUNKS; i++) {

		chunk_name = "L" + std::to_string(final_merge_phase+1) + "chunk" +  std::to_string(i);
		update_superblock(sb,PM_SORT_SPLIT_RESULT,i);
		if(cb_create_key(work_key,
				   chunk_name,
				   PM_SORT_CHUNK_SIZE,
				   FLAGS_NO_IMPLICIT_UNLOCK,
				   value_addr,
				   &key_addr,
				   &key_handle) != S_OK)
			throw General_exception("create key");;
		PM_chunk = static_cast<ADO_cpp_pm_sort_plugin_record *>(value_addr);
		RAM_chunk = (*data_map)[chunk_name];
		pmem_memcpy_persist(PM_chunk, RAM_chunk, PM_SORT_CHUNK_SIZE);

		if (cb_unlock(work_key, key_handle) != S_OK)
			throw General_exception("unlocking key");

	}

	return S_OK;
}

// sort on RAM with recovery from start of phase
status_t ADO_cpp_pm_sort_plugin::sort_task2(ADO_cpp_pm_sort_plugin_superblock *sb, const uint64_t work_key)
{
	bool quicksort_needed = false;
	bool merge_needed  = false;
	uint64_t merge_phase = 1;
	uint64_t chunk_index = 0;
	int level = 0;

	void * out_value_addr = nullptr;
	size_t value_size = 0;
	component::IKVStore::key_t key_handle;
	std::string key_name;

	sort_start = now_ms();
	ADO_cpp_pm_sort_plugin_superblock * updated_sb = get_updated_superblock(sb);
	std::map<std::string, ADO_cpp_pm_sort_plugin_record*> data_map;

	//check recovery location and restore PM backup to a stable state, then copy PM to map on RAM
	if (updated_sb->state == PM_SORT_IDLE){
		// Start sorting from scratch
		quicksort_needed = true;
		merge_needed = true;
		level = 0;
	}
	else if (updated_sb->state == PM_SORT_CHUNK){
		quicksort_needed = true;
		merge_needed = true;
		level = 0;
		//make sure no L0chunkXSorted chunks on PM (from half-written result of this phase)
		stabilize_PM(work_key,level);
	}
	else if (updated_sb->state == PM_SORT_MERGE){
		//continue from start of current merge phase
		merge_needed = true;
		merge_phase = updated_sb->merge_phase;
		//make sure no chunks on PM from levels L(merge_phase - 2) or L(merge_phase).
		level = (merge_phase == 1)? -1:(int)(merge_phase-1);
		stabilize_PM(work_key,level);
	}
	else{ // updated_sb->state == PM_SORT_SPLIT_RESULT

		level = (int)get_final_merge_phase();
		chunk_index = updated_sb->last_modified_index;
		/*compute how many merge phases happened for appropriate name for final result*/
		key_name = "L"+ std::to_string(level +1) +"chunk" + std::to_string(chunk_index);
		/*delete chunk in chunk_index in case it was created but not writen into completely*/
		if(cb_open_key(work_key,
			       key_name,
			       FLAGS_NO_IMPLICIT_UNLOCK,
			       out_value_addr,
			       value_size,
			       nullptr,
			       &key_handle) == S_OK){
			if (cb_unlock(work_key, key_handle) != S_OK)
				throw General_exception("unlocking key");
			if (cb_erase_key(key_name) != S_OK)
				throw General_exception("erasing key");
		}
	}
	//here we are guaranteed PM contains all the chunks from level "level" and only them
	//load these chunks into map.
	load_from_PM(work_key, level, &data_map);
	if (quicksort_needed){
		update_superblock(sb,PM_SORT_CHUNK,0);
		sort_chunks(sb, work_key, 0, 2, &data_map);
		//write L0chunkXSorted chunks to PM
		backup_to_PM(work_key, 0, &data_map);
		merge_phase = 1;
		update_superblock(sb,PM_SORT_MERGE,0,merge_phase);
		//delete L0chunkX chunks from PM
		level = -1;
		stabilize_PM(work_key,level);
		quicksort_done = now_ms();
	}
	if (merge_needed) {
		merge(sb, work_key, 0, merge_phase, 2, &data_map);
		chunk_index = 0;
		update_superblock(sb,PM_SORT_SPLIT_RESULT,chunk_index);
		//leave only final merge result in PM
		level = (int)get_final_merge_phase();
		stabilize_PM(work_key,level);
		merge_done = now_ms();
	}
	// split results
	split_result(sb, work_key, chunk_index, 2, &data_map);
	//write to PM the final result (L final_merge_phase+1)
	backup_final_result(sb, work_key,chunk_index,&data_map);
	update_superblock(sb,PM_SORT_IDLE);
	level = (int)get_final_merge_phase();
	//delete L(final_merge_phase) chunk from PM
	stabilize_PM(work_key,level+1);

	//free chunks in map
	free_map(&data_map);
	return S_OK;

}

status_t ADO_cpp_pm_sort_plugin::do_work(const uint64_t work_key,
					 const char * key,
					 size_t key_len,
					 IADO_plugin::value_space_t& values,
					 const void *in_work_request,
					 const size_t in_work_request_len,
					 bool new_root,
					 response_buffer_vector_t& response_buffers)
{

	component::IKVStore::key_t superblock_handle;
	std::string err;
	using namespace flatbuffers;

	auto msg = GetMessage(in_work_request);

	// Sort
	if (msg->element_as_SortRequest()) {

		void * out_value_addr = nullptr;
		size_t value_size = 0;
		ADO_cpp_pm_sort_plugin_superblock *sb;

		if(cb_open_key(work_key,
			       SUPERBLOCK_CHUNK,
			       FLAGS_NO_IMPLICIT_UNLOCK,
			       out_value_addr,
			       value_size,
			       nullptr,
			       &superblock_handle) != S_OK)
			throw General_exception("failed opening superblock");

		sb = (struct ADO_cpp_pm_sort_plugin_superblock *)out_value_addr;

		auto sr = msg->element_as_SortRequest();
		auto type = sr->type();
		PLOG("Got sort type %u", type);
		if (type == 1)
			sort_task1(work_key);
		else if (type == 2)
			sort_task2(sb, work_key);
		else if (type == 3)
			sort_task3(sb, work_key);

		PLOG("Chunk sort took %lums", quicksort_done - sort_start);
		PLOG("Chunk merge took %lums", merge_done - quicksort_done);
		PLOG("Total sort time: %lums", now_ms() - sort_start);

		if(cb_unlock(work_key, superblock_handle) != S_OK)
			throw General_exception("unlocking key");
	}
	//Init
	else if(msg->element_as_InitRequest()) {
		PLOG("Got init");
		init(work_key);
	}
	//Verify
	else if(msg->element_as_VerifyRequest()) {
		PLOG("Got verify");
		if (!verify(work_key)) {
			PERR("Result not sorted!");
		} else {
			PLOG("verify OK");
		}
	}
	else {
		PLOG("got something unrecognized!");
	}

	return S_OK;
}

status_t ADO_cpp_pm_sort_plugin::shutdown()
{
	/* here you would put graceful shutdown code if any */
	return S_OK;
}


/**
 * Factory-less entry point
 *
 */
extern "C" void * factory_createInstance(component::uuid_t interface_iid)
{
	if(interface_iid == interface::ado_plugin)
		return static_cast<void*>(new ADO_cpp_pm_sort_plugin());
	else return NULL;
}
