/*
	MT ops
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/
#ifndef __mt__hpp__
#define __mt__hpp__
#include "types.hpp"
 
enum {
	mt_stat_idle = 0U,
	mt_stat_busy = 1U,
	__mt_u32__ = 0xfffffffeU
};

/*
	A basic  thread tile extent.S0 = base offset , S1 = end offset.Length = S1-S0
*/
struct mt_extent_t {                                                          
    uint32_t s0,s1;
    mt_extent_t() : s0(0U),s1(0U) {}
    mt_extent_t(const uint32_t in_s0,const uint32_t in_s1) : s0(in_s0) , s1(in_s1) {}
};

bool mt_calculate_extent(std::vector<mt_extent_t>& result,const uint32_t slice_len,const uint32_t thread_count);
bool mt_init(const uint32_t thread_count);
bool mt_call_thread(void* (entry_point)(void*),void* my_arg,const uint32_t which);
bool mt_wait_threads(const uint32_t active_threads);
uint32_t mt_get_thread_contexts();
void mt_shutdown();
void mt_list(std::vector<uint32_t>& res,const uint32_t caller,const uint32_t wanted_stat,const uint32_t set_stat,
						const uint32_t max_list_size);
void mt_wr_stat(const uint32_t id,const uint32_t stat);
uint32_t mt_rd_stat(const uint32_t id);


typedef void* mt_entry_point_result_t;
#define mt_entry_point_stack_args void* __in_epi_args__
#define mt_entry_point_prologue(__args__,__type__) { __args__ = (__type__*)__in_epi_args__; }
#define mt_entry_point_epilogue(__ret__) { pthread_exit(__ret__); return __ret__; }
#endif

