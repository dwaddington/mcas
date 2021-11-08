/*
 * Description  : PM sorting ADO
 * Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
 * Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
 * License      : Apache License, Version 2.0
 * Done as part of the Workshop on Search Data Structures for High-Performance Databases
 * at Tel-Aviv University.
 */

#ifndef __CPP_PM_SORT_PLUGIN_H__
#define __CPP_PM_SORT_PLUGIN_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <common/cycles.h>
#include <api/ado_itf.h>
#include <cpp_pm_sort_proto_generated.h>
#include <libpmem.h>
#include <string.h>
#include <list>
#include <common/utils.h>

#define PM_SORT_CHUNK_SIZE MiB(128ULL)
#define PM_SORT_RECORD_ARRAY_SIZE(chunk_level) (1280000ULL<<(chunk_level)) /**128 MB chunck, each record is 100 B*/
#define PM_SORT_NUMBER_OF_CHUNKS 512ULL

/**
 * A logging record
 * first 10 bytes are the key for sorting purposes
 */
struct ADO_cpp_pm_sort_plugin_record
{
	uint8_t timestamp[5];
	uint8_t datacenter_id[2];
	uint8_t operation_type[3];
	uint8_t src_address[4];
	uint8_t dst_address[4];
	uint8_t private_data[82];
};

#define PM_SORT_RECORD_SIZE sizeof(ADO_cpp_pm_sort_plugin_record)

enum pm_sorting_state_t {
	PM_SORT_IDLE,
	PM_SORT_CHUNK,
	PM_SORT_MERGE,
	PM_SORT_SPLIT_RESULT,
};

/**
 * PM sort superblock
 */
struct ADO_cpp_pm_sort_plugin_superblock
{
	bool valid;
	uint64_t timestamp;
	enum pm_sorting_state_t state;
	uint64_t last_modified_index;
	/* only relevant if in state PM_SORT_MERGE*/
	uint64_t merge_phase;
};

#define PM_SORT_SUPERBLOCK_SIZE (sizeof(struct ADO_cpp_pm_sort_plugin_superblock))
#define PM_SORT_NR_SUPERBLOCKS (2)
#define PM_SORT_SUPERBLOCKS_SIZE (PM_SORT_NR_SUPERBLOCKS * PM_SORT_SUPERBLOCK_SIZE)

#define POOL_SIZE 3*((PM_SORT_NUMBER_OF_CHUNKS+1) * PM_SORT_CHUNK_SIZE) + PM_SORT_SUPERBLOCKS_SIZE

class ADO_cpp_pm_sort_plugin : public component::IADO_plugin
{
private:
	static constexpr bool option_DEBUG = true;

	/**
	 * Updates the superblocks to the new state via the following steps:
	 * Foreach superblock by order do:
	 *	1. Toggle the valid flag for the superblock and persist the state.
	 *	2. Update the metadata of the superblock and persist the state.
	 *	3. Toggle back the valid flag for the superblock and persist that state.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param enum pm_sorting_state_t the new state to update to.
	 * @param uint64_t the last chunk index modified by the algorithm, used for recovery.
	 * @param uint64_t the last persistent merge phase, used for recovery.
	 */
	void update_superblock(ADO_cpp_pm_sort_plugin_superblock *sb,
			       enum pm_sorting_state_t state,
			       uint64_t last_modified_index,
			       uint64_t merge_phase);

	/**
	 * Swap portion of the quicksort, swaps two given records.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the current chunk.
	 * @param int index of the first record to swap.
	 * @param int index of the second record to swap.
	 */
	void swap(ADO_cpp_pm_sort_plugin_superblock *sb, ADO_cpp_pm_sort_plugin_record *records, int i, int j);

	/**
	 * Comapres two records by their key (first 10 bytes).
	 * @param ADO_cpp_pm_sort_plugin_record *points to the first record.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the second record.
	 * @return 1 if rec1's key is smaller, otherwise 0.
	 */
	int is_smaller(ADO_cpp_pm_sort_plugin_record *rec1, ADO_cpp_pm_sort_plugin_record *rec2);
	
	/**
	 * Partition portion of the quicksort algorthim, the pivot element
	 * in each Partition phase is chosen according to the median of three
	 * strategy, i.e, The median between the first, middle, and last record
	 * in the given range.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the chunks (array of records).
	 * @param int start of the range to partition.
	 * @param int end of the range to partition.
	 * @return index of the pivot after partitioning the range.
	 */
	int partition(ADO_cpp_pm_sort_plugin_superblock *sb, ADO_cpp_pm_sort_plugin_record *chunk, int start, int end);

	/**
	 * Recursive portion of the quicksort algorthim and the begining of it, 
	 * runs until `start >= end`.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the chunks (array of records).
	 * @param int start of the range to sort.
	 * @param int end of the range to sort.
	 */
	void quicksort(ADO_cpp_pm_sort_plugin_superblock *sb, ADO_cpp_pm_sort_plugin_record *chunk, int start, int end);

	/**
	 * Given a chunk X, create a new XSorted chunk and update the superblock accordingly
	 * for the given task.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the chunks (array of records).
	 * @param Aconst std::string& the name of the chunk (if name is X, new chunk shall be XSorted).
	 * @param uint64_t MCAS ADO work_key.
	 * @param int task type, affects how the superblock is updated and where the new chunk is written to.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map from names to chunks, used only
	 *	  in tasks that use the RAM (tasks 1 & 2).
	 */
	void create_sorted_chunk(ADO_cpp_pm_sort_plugin_superblock *sb,
				 ADO_cpp_pm_sort_plugin_record *chunk,
				 const std::string& chunk_name,
				 uint64_t work_key,
				 int task = 3,
				 std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map = nullptr);

	/**
	 * Runs though the final data and verifies that it's sorted.
	 * @param uint64_t MCAS ADO work_key.
	 * @return true if the result is sorted, otherwise false.
	 */
	bool verify(uint64_t work_key);

	/**
	 * Calculates the final merge phase level.
	 * @return the final merge phase level.
	 */
	uint64_t get_final_merge_phase();
	
	/**
	 * Merge two sorted chunks.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the destination chunk.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the first source chunk.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the second source chunk.
	 * @param uint64_t current merge phase (determines expected chunk sizes).
	 */
	void chunk_merge(ADO_cpp_pm_sort_plugin_record *dst_chunk,
			 ADO_cpp_pm_sort_plugin_record *src_chunk_1,
			 ADO_cpp_pm_sort_plugin_record *src_chunk_2,
			 uint64_t merge_phase);

	/**
	 * Iterates of the superblocks and returns the valid state.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t the first valid superblock, NULL if non was found (should never happen).
	 */
	ADO_cpp_pm_sort_plugin_superblock * get_updated_superblock(ADO_cpp_pm_sort_plugin_superblock *sb);

	/**
	 * Handles the sorting phase of the ADO, allocates dest chunks according to the task and
	 * persists the output if needed.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t MCAS ADO work_key.
	 * @param uint64_t Chunk index to begin sorting from.
	 * @param int task type, affects how the superblock is updated and where the new chunk is written to.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map from names to chunks, used only
	 *	  in tasks that use the RAM (tasks 1 & 2).
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t sort_chunks(ADO_cpp_pm_sort_plugin_superblock *sb,
			     const uint64_t work_key,
			     uint64_t chunk_index,
			     int task = 3,
			     std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map = nullptr);

	/**
	 * Handles the merging phase of the ADO, runs through all merge levels and
	 * updates the superblocks between phases if needed.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t MCAS ADO work_key.
	 * @param uint64_t Chunk index to begin merging from.
	 * @param uint64_t Merge level to begin merging from.
	 * @param int task type, affects how the superblock is updated and where the new chunks are written to.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map from names to chunks, used only
	 *	  in tasks that use the RAM (tasks 1 & 2).
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t merge(ADO_cpp_pm_sort_plugin_superblock *sb,
		       const uint64_t work_key,
		       uint64_t chunk_index,
		       uint64_t start_merge_phase,
		       int task = 3,
		       std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map = nullptr);

	/**
	 * Splits the final result chunk into many L0 sized chunks (in the same order as they
	 * appear in the final chunk).
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t MCAS ADO work_key.
	 * @param uint64_t Chunk index to begin splitting to (not 0 during recovery).
	 * @param int task type, affects how the superblock is updated and where the new chunks are written to.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map from names to chunks, used only
	 *	  in tasks that use the RAM (tasks 1 & 2).
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t split_result(ADO_cpp_pm_sort_plugin_superblock *sb,
			       const uint64_t work_key,
			       uint64_t chunk_index,
			       int task = 3,
			       std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map = nullptr);

	/**
	 * Inits the ADO, should be the first thing called.
	 * Reads the superblock and checks if recovery should be triggered, if not clears
	 * the pools and rewrites the unsorted data to the PM.
	 * @param uint64_t MCAS ADO work_key.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t init(const uint64_t work_key);

	/**
	 * Loads the unsorted gensort chunks from storage to the PM.
	 * @param uint64_t MCAS ADO work_key.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t load_data(const uint64_t work_key, ADO_cpp_pm_sort_plugin_superblock *sb);

	/**
	 * Clears the pool from old data.
	 * @param uint64_t MCAS ADO work_key.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t clear_pool(const uint64_t work_key);

	/**
	 * Retrieves the max between two records in a given chunk
	 * @param ADO_cpp_pm_sort_plugin_record *points to the chunk.
	 * @param int index of the first record.
	 * @param int index of the second record record.
	 * @param int index of the maximal record of the two.
	 */
	int max_record(ADO_cpp_pm_sort_plugin_record *chunk, int a, int b);

	/**
	 * Retrieves the min between two records in a given chunk
	 * @param ADO_cpp_pm_sort_plugin_record *points to the chunk.
	 * @param int index of the first record.
	 * @param int index of the second record record.
	 * @param int index of the minimal record of the two.
	 */
	int min_record(ADO_cpp_pm_sort_plugin_record *chunk, int a, int b);

	/**
	 * Calculates the pivot for quicksort according to the median of three
	 * strategy, i.e, The median between the first, middle, and last record
	 * in the given range.
	 * @param ADO_cpp_pm_sort_plugin_record *points to the chunk.
	 * @param int begining of the range.
	 * @param int end of the range.
	 * @param int index of the pivot element.
	 */
	int get_pivot(ADO_cpp_pm_sort_plugin_record *chunk, int start, int end);

	/**
	 * Loads chunks from PM to RAM.
	 * @param uint64_t MCAS ADO work_key.
	 * @param int chunk level to load.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map to store the chunks.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t load_from_PM(const uint64_t work_key, int level, std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map);

	/**
	 * Frees the RAM data map (load_from_PM output).
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map to free.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t free_map(std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map);

	/**
	 * Delete from PM old key-value pairs from past phases, should only be called
	 * after current phase has been properly persisted.
	 * also used in crash recovery to return PM to a stable state.
	 * @param uint64_t MCAS ADO work_key.
	 * @param int target chunk level (older levels will be deleted).
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t stabilize_PM(const uint64_t work_key, int level);

	/**
	 * Copies all chunks from RAM map to PM
	 * @param uint64_t MCAS ADO work_key.
	 * @param int chunk level in map.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map of chunks.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t backup_to_PM(const uint64_t work_key, uint64_t level, std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *data_map);

	/**
	 * Task 2 specific function, writes final result from map, starting from chunk_index, and updates superblock.
	 * This allows task 2 recovery to continue from the persisted state.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t MCAS ADO work_key.
	 * @param int chunk index to start from.
	 * @param std::map<std::string, ADO_cpp_pm_sort_plugin_record*> *map of final chunks.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t backup_final_result(ADO_cpp_pm_sort_plugin_superblock *sb,
				     const uint64_t work_key,
				     uint64_t chunk_index,
				     std::map<std::string,
				     ADO_cpp_pm_sort_plugin_record*> *data_map);

	/**
	 * Sort according to task 1 logic (RAM use only)
	 * @param uint64_t MCAS ADO work_key.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t sort_task1(const uint64_t work_key);

	/**
	 * Sort according to task 2 logic (RAM sorting & PM flushing)
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t MCAS ADO work_key.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t sort_task2(ADO_cpp_pm_sort_plugin_superblock *sb, const uint64_t work_key);
	
	/**
	 * Sort according to task 3 logic (PM use only), continues from superblock state.
	 * @param ADO_cpp_pm_sort_plugin_superblock *points to the superblocks.
	 * @param uint64_t MCAS ADO work_key.
	 * @param status_t returns S_OK if no error, otherwise throws execptions.
	 */
	status_t sort_task3(ADO_cpp_pm_sort_plugin_superblock *sb, const uint64_t work_key);
public:
	/**
	 * Constructor
	 *
	 * @param block_device Block device interface
	 *
	 */
	ADO_cpp_pm_sort_plugin() {}

	/**
	 * Destructor
	 *
	 */
	virtual ~ADO_cpp_pm_sort_plugin() {}

	/**
	 * Component/interface management
	 *
	 */
	DECLARE_VERSION(0.1f);
	DECLARE_COMPONENT_UUID(0x84307b41,0x84f1,0x496d,0xbfbf,0xb5,0xe4,0xcf,0x40,0x70,0xe3);

	void * query_interface(component::uuid_t& itf_uuid) override {
	if (itf_uuid == component::IADO_plugin::iid()) {
		return (void *) static_cast<component::IADO_plugin*>(this);
	} else
		return NULL; // we don't support this interface
	}

	void unload() override {
		delete this;
	}

public:

	/* IADO_plugin */
	status_t register_mapped_memory(void * shard_vaddr,
					void * local_vaddr,
					size_t len) override;

	status_t do_work(const uint64_t work_key,
			 const char * key,
			 size_t key_len,
			 IADO_plugin::value_space_t& values,
			 const void * in_work_request, /* don't use iovec because of non-const */
			 const size_t in_work_request_len,
			 bool new_root,
			 response_buffer_vector_t& response_buffers) override;

	status_t shutdown() override;

};


#pragma GCC diagnostic pop

#endif
