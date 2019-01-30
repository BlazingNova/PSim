#ifndef CACHE_H__
#define CACHE_H__

#include <vector>

typedef enum Cache_ReplPolicy_Enum
{
	LRU 		= 0,
	RAND 		= 1,
	SRRIP 		= 2,
	DRRIP 		= 3,
	SHIP 		= 4,
	NUM_REPL 	= 5
}Cache_ReplPolicy;

class Cache_Line
{
public:
	bool 	valid;
	bool 	dirty;
	uns64 	addr;
	uns64 	tag;
	uns64 	data;
	uns 	core_id;

	Cache_Line();
	~Cache_Line();
};

class Cache
{
public:
	uns64 sets;
	uns64 ways;
	uns64 num_entries;
	Cache_ReplPolicy repl_policy;
	uns64 access_count;
	uns64 hit_count;
	uns64 miss_count;
	
	vector<uns64> set_accesses;
	vector<uns64> set_hits;
	vector<uns64> set_misses;
	vector<uns64> set_last_access;

	//Heap allocs - Must delete
	Cache_Line *entries;
	//End Heap allocs

	Cache();
	Cache(uns64 sets, uns64 ways);
	~Cache();
	
	void cache_init();
	bool cache_probe();
	bool cache_access();
	bool cache_install();
	bool cache_invalidate();
	
	uns64 cache_get_set();
	uns64 cache_get_tag();

	uns64 cache_find_victim();
	uns64 cache_find_victim_lru();
	uns64 cache_find_victim_rand();
}

#endif
