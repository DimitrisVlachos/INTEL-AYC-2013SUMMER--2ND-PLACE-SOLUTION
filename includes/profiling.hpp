
#ifndef _profiling_hpp_
#define _profiling_hpp_
#define _profile_code_

#ifdef _profile_code_
/*
    Author : Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/
#include "types.hpp"
#include <ctime>
void profiler_init(boolean_t silent);
void profiler_shutdown();
void profiler_update(const std::string& scope_desc,const f64_t& delta);
void profiler_dgb_dump(const std::string& file_path);

class scoped_profiler_c { 
    private:
    std::string m_scope_desc;
    time_t m_start;
    time_t m_end;

    public:
    scoped_profiler_c() {
        printf("Bad usage!\n");
        assert(0);
    }

    scoped_profiler_c(const char* scope_desc) : m_scope_desc(scope_desc) {
        printf("[enter %s]\n",scope_desc);
        m_start = std::clock();
    }
    scoped_profiler_c(const std::string& scope_desc) : m_scope_desc(scope_desc) {
        printf("[enter %s]\n",scope_desc.c_str());
        m_start = std::clock();
    }

    ~scoped_profiler_c() {
        m_end = std::clock();
        profiler_update(m_scope_desc,(const f64_t)(m_end-m_start)/(const f64_t)CLOCKS_PER_SEC);
        printf("[leave %s]\n",m_scope_desc.c_str());
    }
};

#define profiler_profile_me(...) scoped_profiler_c _tmp_prof_(__func__) 
#define profiler_profile_me_ex(_m_) scoped_profiler_c _tmp_prof_(_m_) 
#else
#define profiler_profile_me(...)
#define profiler_profile_me_ex(...)
#define profiler_init(...)
#define profiler_shutdown(...)
#define profiler_dgb_dump(...)
#endif

#endif

