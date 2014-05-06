
/*
	Pixel signature generator
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#ifndef __pixel_signature_hpp__
#define __pixel_signature_hpp__

#include "types.hpp"
#include "pixel_chunk.hpp"

struct pixel_sig_field_ctx_t {
	uint32_t rel_y;
	pixel_chunk_t* chunk;
	pixel_sig_field_ctx_t() : rel_y(0),chunk(0) {}
	pixel_sig_field_ctx_t(uint32_t ry,pixel_chunk_t* p_chunk) : rel_y(ry),chunk(p_chunk) {}
};

struct pixel_signature_t {
	uint32_t complexity;
	uint32_t fields;
	std::vector<pixel_sig_field_ctx_t> sig_list;
};

pixel_signature_t* pixel_signature_new(pixel_chunk_t* in_chunk,const uint32_t tile_complexity,const uint32_t fields);
void pixel_signature_delete(pixel_signature_t* ps);
#endif


