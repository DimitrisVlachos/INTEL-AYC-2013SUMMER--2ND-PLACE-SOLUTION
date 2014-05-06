/*
    Author : Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#include "profiling.hpp"
#ifdef _profile_code_
struct scope_info_t {
    std::string desc;
    f64_t delta; 
    uint64_t calls;
    scope_info_t(const std::string& in_desc) : desc(in_desc) , delta(0.0),calls(0) {}
};

std::vector<scope_info_t> scope_info;
boolean_t g_b_silent;

static inline f64_t s_to_m(const f64_t& s) {
    return ((s + 0.00001) / 60.f);
}

static scope_info_t* grab_scope_entry(const std::string& s) { /*XXX not thread safe*/
    for (uint32_t i = 0U,j = (uint32_t)scope_info.size();i < j;++i) {
        if (scope_info[i].desc == s) {
            return &scope_info[i];
        }
    }

    const uint32_t old_size = scope_info.size();
    scope_info.push_back(scope_info_t(s));
    if (scope_info.size() > old_size) {
        return &scope_info[old_size];
    }
    return 0;
}

void profiler_init(boolean_t silent) {
    scope_info.clear();
    g_b_silent = silent;
}

void profiler_shutdown() {
    profiler_dgb_dump("Profiling_Results.txt");
    scope_info.clear();
}

void profiler_update(const std::string& scope_desc,const f64_t& delta) { /*XXX not thread safe*/
    scope_info_t* inf = grab_scope_entry(scope_desc);
    if (0 == inf) {
        return;
    }

    inf->delta += delta;
    ++inf->calls;

    if (g_b_silent) {
        return;
    }

    printf("[%s][Took : %.3fms][Total : %.3fsec %.1fmin]\n",scope_desc.c_str(),
    delta,inf->delta,s_to_m(inf->delta));
}

void profiler_dgb_dump(const std::string& file_path) {
    FILE* f;

    f = fopen(file_path.c_str(),"w");

    printf("\n\nPROFILER STATS DUMP BEGIN (%u entries)\n\n",(uint32_t)scope_info.size());
    if (f) {
        fprintf(f,"\n\nPROFILER STATS DUMP BEGIN (%u entries)\n\n",(uint32_t)scope_info.size());
    }
    for (uint32_t i = 0;i < (uint32_t)scope_info.size();++i) {
        printf("Func : %s | Calls : %lu | Time spent %.5f seconds %.1f minutes \n",
        scope_info[i].desc.c_str(),scope_info[i].calls,scope_info[i].delta,s_to_m(scope_info[i].delta));

        if (f) {
            fprintf(f,"Func : %s | Calls : %lu | Time spent %.5f seconds %.1f minutes \n",
            scope_info[i].desc.c_str(),scope_info[i].calls,scope_info[i].delta,s_to_m(scope_info[i].delta));
        }
    }

    if (f) {
        fprintf(f,"\n\nPROFILER STATS DUMP END\n\n");
        printf("Results saved to %s\n",file_path.c_str());
        fclose(f);
    }
    printf("\n\nPROFILER STATS DUMP END\n\n");
}
#endif

