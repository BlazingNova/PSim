#include "cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/////////////////////////////////////////
/////Cache_Line FUNCTIONS START HERE/////
/////////////////////////////////////////

//Cache Line Constructor - num_sets all fields to default state
Cache_Line::Cache_Line()
{
	valid 	= false;
	dirty 	= false;
	addr 	= 0;
	tag 	= 0;
	data 	= 0;
	core_id = 0;
}

/////////////////////////////////////////
/////////////////////////////////////////

//Cache Line Destructor - No dynamic elements here
Cache_Line::~Cache_Line()
{

}

/////////////////////////////////////////
//////Cache_Line FUNCTIONS END HERE//////
/////////////////////////////////////////

////////////////////////////////////
/////Cache FUNCTIONS START HERE/////
////////////////////////////////////

//Default Constructor - I do not use this
Cache::Cache()
{

}

/////////////////////////////////////////
/////////////////////////////////////////

//Parameterized Constructor - This is what is used *alnum_ways*
Cache::Cache(uns64 num_sets, uns64 num_ways, uns64 repl_policy)
{
	this->num_sets 			= num_sets;
	this->num_ways 			= num_ways;
	this->num_entries 		= num_sets*num_ways;
	this->repl_policy 		= repl_policy;
	access_count 			= 0;
	hit_count 				= 0;
	miss_count 				= 0;

	set_accesses.resize(num_sets);
	set_hits.resize(num_sets);
	set_misses.resize(num_sets);
	set_last_access.resize(num_sets);

	//Dynamic allocs
	entries 		= new Cache_Line[num_entries];
}

/////////////////////////////////////////
/////////////////////////////////////////

//Destructor - Delete dynamic elements to prevent leaks
Cache::~Cache()
{
	delete [] entries;
}

/////////////////////////////////////////
/////////////////////////////////////////

//This function returns the set index that the line address passed to the function goes to
uns64 Cache::cache_get_set(uns64 addr)
{
	return addr%num_sets;
}

/////////////////////////////////////////
/////////////////////////////////////////

//This function returns the set index that the line address passed to the function goes to
uns64 Cache::cache_get_tag(uns64 addr)
{
	return addr;
}

/////////////////////////////////////////
/////////////////////////////////////////

//This function returns if the line is present in the cache, but does not update repl/access stats
bool Cache::cache_probe(uns64 addr)
{
	uns64 set 	= cache_get_set(addr);
	uns64 tag 	= cache_get_tag(addr);
	uns64 begin = set*num_ways;
	uns64 end 	= begin+num_ways;

	Cache_Line *entry;
	//First check if it was the last accessed
	entry = &entries[set_last_access[set]];
	if((entry->valid)&&(entry->tag==tag))
	{
		return true;
	}
	//Was not the last accessed line of the set
	for(uns64 ii=begin;ii<end;ii++)
	{
		entry = &entries[ii];
		if((entry->valid)&&(entry->tag==tag))
		{
			return true;
		}
	}
	//Was not in the entire set
	return false;
}

/////////////////////////////////////////
/////////////////////////////////////////

//This function performs the cache access, performs replacement state update and H/M stat update
bool Cache::cache_access(uns64 addr, bool write)
{
	uns64 set 	= cache_get_set(addr);
	uns64 tag 	= cache_get_tag(addr);
	uns64 begin = set*num_ways;
	uns64 end 	= begin+num_ways;

	access_count++;
	Cache_Line *entry;
	//First check if it was the last accessed
	entry = &entries[set_last_access[set]];
	if((entry->valid)&&(entry->tag==tag))
	{
		set_hits[set]++;
		hit_count++;
		//If already dirty, retain state, else set dirty
		dirty = dirty|write;
		return true;
	}
	//Was not the last accessed line of the set
	for(uns64 ii=begin;ii<end;ii++)
	{
		entry = &entries[ii];
		if((entry->valid)&&(entry->tag==tag))
		{
			set_hits[set]++;
			hit_count++;
			//If already dirty, retain state, else set dirty
			dirty = dirty|write;
			set_last_access[set]=ii;
			return true;
		}
	}
	//Was not in the entire set
	set_misses[set]++;
	miss_count++;
	return false;
}

/////////////////////////////////////////
/////////////////////////////////////////

//This function invalidates the cache line if it is present
bool Cache::cache_invalidate(uns64 addr)
{
	uns64 set 	= cache_get_set(addr);
	uns64 tag 	= cache_get_tag(addr);
	uns64 begin = set*num_ways;
	uns64 end 	= begin+num_ways;

	Cache_Line *entry;	
	for(uns64 ii=begin;ii<end;ii++)
	{
		entry = &entries[ii];
		if((entry->valid)&&(entry->tag==tag))
		{
			entry->valid = false;
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////
/////////////////////////////////////////

bool Cache::cache_install(uns64 addr)
{
	uns64 set 	= cache_get_set(addr);
	uns64 tag 	= cache_get_tag(addr);
	uns64 begin = set*num_ways;
	uns64 end 	= begin+num_ways;

	Cache_Line *entry;
	for(uns64 ii=begin;ii<end;ii++)
	{
		entry = &entries[ii];
		if((entry->valid)&&(entry->tag==tag))
		{
			entry->valid = false;
			return false;
			//Maybe assert this / Double install not really correctness issue
		}
	}	

	victim 			= cache_find_victim(set);
	entry 			= entries[victim];
	
	entry->valid 	= true;
	entry->dirty 	= false;
	entry->addr 	= addr;
	entry->tag 		= tag;
	entry->data 	= 0;
	entry->core_id 	= 0;
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_find_victim(uns64 set)
{
	uns64 begin = set*num_ways;
	uns64 end 	= begin+num_ways;
	
	Cache_Line *entry;
	
	//Find invalid first!
	for(uns64 ii=begin;ii<end;ii++)
	{
		entry=&entries[ii];
		if(entry->valid==false)
		{
			return ii;
		}
	}

	switch(repl_policy)
	{
		case LRU:
			return cache_find_victim_lru(set);
		case RAND:
			return cache_find_victim_rand(set);
		case SRRIP:
			return cache_find_victim_srrip(set);
		case DRRIP:
			return cache_find_victim_drrip(set);
		case SHIP:
			return cache_find_victim_ship(set);
		default:
			//FIXME : ASSERT OUT HERE
			break;
	}
	//FIXME : ASSERT OUT HERE
	return 0;
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_find_victim_lru(uns64 set)
{
	uns64 begin = set*num_ways;
	uns64 end = begin+num_ways;

	uns64 victim = start;
	for(uns64 ii=start+1;ii<end;ii++)
	{
		if(entries[ii].lru<entries[victim].lru) //If ii older than victim
		{
			victim = ii;
		}
	}
	return victim;
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_find_victim_rand(uns64 set)
{
	uns64 victim = (set*num_ways)+(rand()%num_ways);
	return victim;
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_find_victim_srrip(uns64 set)
{
	uns64 begin = set*num_ways;
	uns64 end = begin+num_ways;
	for(uns64 ii=start;ii<end;ii++)
	{
		if(entries[ii].rrpv==RRPV_MAX)
		{
			return ii;
		}
	}
	for(uns64 ii=start;ii<end;ii++)
	{
		entries[ii].rrpv++;
	}
	//Recursive call max times = RRPV_MAX(Should be stack safe)
	return cache_find_victim_srrip(set);
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_find_victim_drrip(uns64 set)
{
	//Victim selection of DRRIP is the same as SRRIP
	return cache_find_victim_srrip(set);
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_find_victim_ship(uns64 set)
{

}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_repl_update(uns64 line, bool insert)
{
	uns64 begin = set*num_ways;
	uns64 end 	= begin+num_ways;

	switch(repl_policy)
	{
		case LRU:
			return cache_repl_update_lru(line,insert);
		case RAND:
			return;
		case SRRIP:
			return cache_repl_update_srrip(line,insert);
		case DRRIP:
			return cache_repl_update_drrip(line,insert);
		case SHIP:
			return cache_repl_update_ship(line,insert);
		default:
			//FIXME : ASSERT OUT HERE
			break;
	}
	//FIXME : ASSERT OUT HERE
	return 0;
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_repl_update_lru(uns64 line, bool insert)
{
	entries[line].lru=cycle;
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_repl_update_srrip(uns64 line, bool insert)
{
	if(insert)
	{
		entries[line].rrpv=RRPV_MAX-1;
	}
	else
	{
		entries[line].rrpv=0;
	}
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_repl_update_drrip(uns64 line)
{
	if(insert)
	{
		if(set%LEADER_DISTANCE==0)//SRRIP
		{
			entries[line].rrpv=RRPV_MAX-1;
		}
		else if(set%LEADER_DISTANCE==1)//BRRIP
		{
			if(rand()%100<BRRIP_THRESHOLD)
			{
				entries[line].rrpv=RRPV_MAX-1;
			}
			else
			{
				entries[line].rrpv=RRPV_MAX-2;
			}
		}
		else
		{
			if(DRRIP_POLICY==0)//SRRIP
			{
				entries[line].rrpv=RRPV_MAX-1;
			}
			else//BRRIP
			{
				if(rand()%100<BRRIP_THRESHOLD)
				{
					entries[line].rrpv=RRPV_MAX-1;
				}
				else
				{
					entries[line].rrpv=RRPV_MAX-2;
				}		
			}
		}
	}
	else//Update on HIT
	{
		entries[line].rrpv=0;
	}
}

/////////////////////////////////////////
/////////////////////////////////////////

uns64 Cache::cache_repl_update_ship(uns64 line)
{
	//TODO : Pending Implementation
}

////////////////////////////////////
//////Cache FUNCTIONS END HERE//////
////////////////////////////////////