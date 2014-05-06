
/*
	Pixel signature generator
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/
#include "pixel_signature.hpp"
#include "aligned_heap.hpp"
#include "mt.hpp"

typedef ptrdiff_t probability_t;

enum cmp_t {
	cmp_eq = 0U,
	cmp_gt,
	cmp_gte,
	cmp_lt,
	cmp_lte,
	__cmp_u32=0xfffffff0
};

struct prob_ctx_t {
	probability_t complexity;
	probability_t fields;
	probability_t* prob;
};


static inline prob_ctx_t* prob_ctx_new(uint32_t complexity,uint32_t fields) {
	probability_t* res;
	prob_ctx_t* pr;
	const uint32_t heap_len = complexity * sizeof(probability_t) * fields;

	res = (probability_t*)heap_new(heap_len);
 	if (!res) {
		printf("prob_ctx_new failed\n");
		assert(0);
		return 0;
	}

	pr = new prob_ctx_t();
	if (!pr) {
		printf("prob_ctx_new failed\n");
		heap_delete(res);
		assert(0);
		return 0;
	}

	pr->complexity = complexity;
	pr->fields = fields;
	pr->prob = res;	
	memset(pr->prob,0,heap_len);
	return pr;
}

static inline void prob_ctx_delete(prob_ctx_t* pr) {
	if (!pr)
		return;

	heap_delete(pr->prob);
	delete pr;
}


static inline probability_t* prob_get(prob_ctx_t* pr,uint32_t field) {
	return &pr->prob[pr->complexity* field];
}

static inline void prob_ctx_zero(prob_ctx_t* pr,const uint32_t field = 0U) {
	if (field >= pr->fields) {
		printf("field >= pr->fields!\n");
		assert(0);
		return;
	}

	uint8_t* p = (uint8_t*)pr->prob;
	memset(p + (pr->complexity*sizeof(probability_t)*field),0,pr->complexity*sizeof(probability_t));
}

static inline pixel_chunk_t* mk_chunk(uint32_t w,uint32_t h) {
	pixel_chunk_t* res;
	res = new pixel_chunk_t();
	res->w = w;
	res->h = h;
	res->bpp = 1;
	res->stride = w;
	res->angle = 0;
	res->compressed = 0;
	res->pixels = (uint8_t*)heap_new(w*h);
	return res;
}

static inline probability_t mulu(probability_t a,probability_t b) {
	for (probability_t d = 0;d < b;++d) {
		const probability_t c = a + a;
		if (c < a) 
			return 1U << (sizeof(probability_t)-1U);
		a = c;
	}
	return a;
}

static probability_t calc_cplex(prob_ctx_t* pr,const uint32_t field) {
	probability_t res = 0U;
	probability_t scale = 0U;

	const probability_t* fa = &pr->prob[field * pr->complexity];

	for (uint32_t i = 0,j = pr->complexity;i < j;++i) {
		const probability_t fetch = *(fa++);
		scale += fetch != 0U;	//Count entropy
		res += fetch;			//Sum probability
	}
 
	return mulu(res + scale,scale);
}

static inline bool cmp_cplex(probability_t cplex_a,probability_t cplex_b,cmp_t cmp) {
	switch (cmp) {
		case cmp_eq:	return (bool)(cplex_a == cplex_b);
		case cmp_gt:	return (bool)(cplex_a > cplex_b);
		case cmp_gte:	return (bool)(cplex_a >= cplex_b);
		case cmp_lt:	return (bool)(cplex_a < cplex_b);
		case cmp_lte:	return (bool)(cplex_a <= cplex_b);
		default:		printf("cmp_cplex unk ?\n"); return false;
	}

	return false;
}
 

pixel_signature_t* pixel_signature_new(pixel_chunk_t* in_chunk,const uint32_t tile_complexity,const uint32_t fields) {

	pixel_signature_t* ps = new pixel_signature_t();
	if (!ps) {
		printf("pixel_signature_new failed\n");
		assert(0);
		return 0;
	}

	ps->complexity = tile_complexity;
	ps->fields = fields;

	prob_ctx_t* prob = prob_ctx_new(256U,1);

	//Support single field list for now 
	pixel_chunk_t* chunk = mk_chunk(in_chunk->w,tile_complexity);  
	ps->sig_list.push_back(pixel_sig_field_ctx_t(0,chunk)); 

	probability_t best_cplex = 0,curr_cplex = 0;
	probability_t* prob_buf = prob_get(prob,0);

	uint32_t best_line = 0U;

	for (uint32_t y = 0,h = in_chunk->h;y < h;++y ) {
		uint32_t y2 = y;
		if (y2 + tile_complexity > h)break;

		uint32_t prev = 0xffff;
		for (uint32_t h2 = y + tile_complexity;y2 < h2;++y2) {
			const uint8_t* line = &in_chunk->pixels[y2 * in_chunk->stride];
			for (uint32_t x = 0,w = in_chunk->w;x < w;++x) {
				uint32_t fetch = *(line++); 
				prob_buf[fetch] += prev != fetch; //better scale   prob based on previous pixel distance?
				prev = fetch;
			}
		}

		curr_cplex = calc_cplex(prob,0);
		if (cmp_cplex(curr_cplex,best_cplex,cmp_gt)) {
			best_cplex = curr_cplex;
			best_line = y;
		}
		y = y2;
		memset(prob_buf,0,256*sizeof(probability_t));
	}
 
	{
		
		//printf("Best line %u : %llu \n",best_line,best_cplex);
		
 		uint8_t* dst = chunk->pixels;

		for (uint32_t y = best_line,h = y + tile_complexity;y < h;++y ) {
			const uint8_t* line = &in_chunk->pixels[y * in_chunk->stride];
			for (uint32_t x = 0,w = in_chunk->w;x < w;++x)
				*(dst++) = *(line++);
		}
	}

	prob_ctx_delete(prob); 
	return ps;
}

void pixel_signature_delete(pixel_signature_t* ps) {
	if (!ps)
		return;

	for (uint32_t i = 0;i < ps->sig_list.size();++i)
		pixel_chunk_delete(ps->sig_list[i].chunk);

	delete ps;
}



