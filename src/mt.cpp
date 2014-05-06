/*
	MT ops
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#include "mt.hpp"
#include "aligned_heap.hpp"

extern "C" {
    #include <pthread.h>
    #include <unistd.h>
}

uint8_t* g_thread_ctx_heap_entry = 0;
pthread_t* g_thread_context = 0;                                          
ptrdiff_t** g_thread_context_res = 0;    
uint32_t* g_thread_stat = 0;                                    
uint32_t g_thread_contexts = 0U;                                                     

static inline /*volatile*/ void mt_lock_op() {
}

static inline /*volatile*/ void mt_unlock_op() {
}

/*
	Returns parameters.nb_threads
*/
uint32_t mt_get_thread_contexts() {
	return g_thread_contexts;
}

/*
	Immediate thread call.Order + Scheduling are not handled
*/
bool mt_call_thread(void* (entry_point)(void*),void* my_arg,const uint32_t which) {
	if (which >= g_thread_contexts) {
		printf("mt_call_thread failed::which >= g_thread_contexts\n");
		assert(0);
		return false;
	} else if (0 != pthread_create(&g_thread_context[which],NULL,entry_point,my_arg)) {
		printf("mt_call_thread::pthread_create failed\n");
		assert(0);
		return false;
	}

	g_thread_stat[which] = mt_stat_busy;

	return true;
}

/*
	Wait for #active_threads to complete their task.Order + Scheduling are not handled
*/
bool mt_wait_threads(const uint32_t active_threads) {
    for (uint32_t i = 0U,j = (active_threads > g_thread_contexts) ? g_thread_contexts : active_threads;i < j;++i) {
        if (0 != pthread_join(g_thread_context[i],(void**)&g_thread_context_res[i])) {
			printf("mt_wait_threads failed\n");
            assert(0);
			return false;
        }
		g_thread_stat[i] = mt_stat_idle;
    }
	return true;
}

/*
	Subdivide input slice to smaller fractions.
*/
bool mt_calculate_extent(std::vector<mt_extent_t>& result,const uint32_t slice_len,const uint32_t thread_count) {
    const uint32_t part_per_slice = (slice_len >= thread_count) ? slice_len / thread_count : 1U;
	//const uint32_t unbalanced_slice = ((slice_len + thread_count) & (~(thread_count - 1U))) - slice_len;

    result.clear();
    result.reserve(thread_count);

    for (uint32_t i = 0U,j = thread_count,start = 0U;i < j;++i) {
        uint32_t end = (start + part_per_slice);
        end = (end > slice_len) ? slice_len : end;
 
        if ((i + 1U) == thread_count) {
            end = slice_len; 
			/*TODO Apply borrowing*/
        }
        
        result.push_back(mt_extent_t(start,end));

        /*Last tile*/
        if (end >= slice_len) {
            break;
        }

        start = end;
    }

	return !result.empty();
}
 
bool mt_init(const uint32_t thread_count) {
 
	const uint32_t cnt = (thread_count == 0U) ? 1U : thread_count;

	g_thread_ctx_heap_entry = (uint8_t*)heap_new(cnt * (sizeof(ptrdiff_t**) + sizeof(pthread_t) + sizeof(uint32_t)));	
	if (g_thread_ctx_heap_entry == 0) {
		printf("MT INIT out of memory!\n");
		return false;
	}
    g_thread_contexts = cnt;
    g_thread_context = (pthread_t*)g_thread_ctx_heap_entry;
    g_thread_context_res = (ptrdiff_t**)&g_thread_ctx_heap_entry[sizeof(pthread_t) * cnt];
	g_thread_stat = (uint32_t*)&g_thread_ctx_heap_entry[(sizeof(ptrdiff_t**) + sizeof(pthread_t)) * cnt];

	for (uint32_t i = 0U;i < cnt;++i) {	
		g_thread_stat[i] = mt_stat_idle;
	}

	return true;
}

void mt_list(std::vector<uint32_t>& res,const uint32_t caller,const uint32_t wanted_stat,const uint32_t set_stat,
						const uint32_t max_list_size) {

	uint32_t added = 0U;
	res.clear();
	res.reserve(max_list_size);

	mt_lock_op();
		for (uint32_t i = 0U,j = caller;i < j;++i) {
			if (g_thread_stat[i] == wanted_stat) {
				g_thread_stat[i] = set_stat;
				res.push_back(i);
				if (++added >= max_list_size) {
					break;
				}
			}
		}

		if (added < max_list_size) {
			for (uint32_t i = caller + 1U,j = (uint32_t)g_thread_contexts;i < j;++i) {
				if (g_thread_stat[i] == wanted_stat) {
					g_thread_stat[i] = set_stat;
					res.push_back(i);
					if (++added >= max_list_size) {
						break;
					}
				}
			}
		}
	mt_unlock_op();
}

void mt_wr_stat(const uint32_t id,const uint32_t stat) { /*Atomic wr*/
	mt_lock_op();
		g_thread_stat[id] = stat;
	mt_unlock_op();
}

uint32_t mt_rd_stat(const uint32_t id) { /*Atomic rd*/
	uint32_t rd_op;

	mt_lock_op();
		rd_op = g_thread_stat[id];
	mt_unlock_op();

	return rd_op;
}

void mt_shutdown() {
	heap_delete(g_thread_ctx_heap_entry); 
	g_thread_ctx_heap_entry = 0;
	g_thread_contexts = 0U;
	g_thread_context = 0;
	g_thread_context_res = 0;
	g_thread_stat = 0;
}

