/*
	Solvers
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

//#define  __MIC__BUILD__

#include "mt.hpp"
#include "aligned_heap.hpp"
#include "solvers.hpp"
#include "profiling.hpp"
#include "sse_msk_luts.h"
#include "pixel_chunk.hpp"
#include "pixel_signature.hpp"

#ifdef __MIC__BUILD__
#include <immintrin.h>

 
 
#endif



typedef pixel_chunk_t* (*xform_stub_t)(pixel_chunk_t*);

struct heap_align_attr mt_solve_entry_point_algo_args_t {
	const uint8_t* atlas_pixels;
	const uint8_t* tile_pixels;
	uint32_t thread_id;
	uint32_t atlas_w,atlas_h;
	uint32_t tile_w,tile_h;
	uint32_t atlas_y0,atlas_y1,atlas_x0,atlas_x1;
	uint32_t line_blocks,block_size,input_list_start,input_list_end;
	int32_t pattern_id;
	const result_t* matches_to_confirm;
	std::vector<result_t> matches;
};
 

static void xform_block_90deg_ccw(const uint8_t* in,uint8_t* out,uint32_t w,uint32_t h);
static void xform_block_90deg_cw(const uint8_t* src,uint8_t* dst,uint32_t w,uint32_t h);
static pixel_chunk_t* xform_identity(pixel_chunk_t* in_chunk);
static pixel_chunk_t* xform_90deg(pixel_chunk_t* in_chunk);
static pixel_chunk_t* xform_180deg(pixel_chunk_t* in_chunk);
static pixel_chunk_t* xform_270deg(pixel_chunk_t* in_chunk);
 
static xform_stub_t g_xofrm_stubs[] = {
	xform_identity,
#ifndef _NO_FIXED_ANGLE_ROTATIONS
	xform_90deg,
	xform_180deg,
	xform_270deg,
#endif
	0
};
 
#ifndef __MIC__BUILD__

/*XEON - SSE*/
static mt_entry_point_result_t mt_solve_entry_point_algo_block32( mt_entry_point_stack_args ) {
	mt_solve_entry_point_algo_args_t* args;

	mt_entry_point_prologue(args,mt_solve_entry_point_algo_args_t); 
	{
		const uint8_t* atlas_pixels = args->atlas_pixels;
		const uint8_t* tile_pixels = args->tile_pixels;
		const uint32_t atlas_y1 = args->atlas_y1,atlas_x1 = args->atlas_x1,atlas_x0 = args->atlas_x0;
		uint32_t y0 = args->atlas_y0;
		const uint32_t a_w=args->atlas_w,b_w=args->tile_w,a_h=args->atlas_h,b_h=args->tile_h;

		/*Precalculate tile pattern edges*/
		const uint32_t tile_safe_w_sub = (b_w != 0U),tile_safe_h_sub = (b_h != 0U);
		const uint32_t tile_mid_point = ((b_h >> 1U) *b_w) + (b_w >> 1U);
		const uint32_t tile_mid_point_0 = ((b_h >> 1U) * b_w) + (b_w-tile_safe_w_sub);
		const uint32_t tile_mid_point_1 = (b_h >> 1U);
		const uint8_t tile_mid_point_pixc = tile_pixels[tile_mid_point];
		const uint8_t tile_spoint_pixc = tile_pixels[0U];
		const uint8_t tile_lpoint_pixc = tile_pixels[b_w - tile_safe_w_sub];
		const uint8_t tile_d2point_pixc = tile_pixels[(b_h-tile_safe_h_sub )*b_w + (b_w-tile_safe_w_sub)];
		const uint8_t tile_dpoint_pixc = tile_pixels[(b_h-tile_safe_h_sub )];
		uint32_t y0_s_base =  y0 * a_w;
		
		bool found;

		/*XXX : NOTE It is expected to go out of thread tile bounds(by + tile->h) if(&only IF) a possible match is found  */
 


		const uint32_t ln_blocks = args->line_blocks;

		std::vector<result_t>& output = args->matches;

		 for (;y0 < atlas_y1;++y0,y0_s_base += a_w) {
			if (((y0 + b_h) ) > a_h) { /*Out of bounds*/
				break; 
			}

			/*Precalculate atlas pattern edges*/
			const uint32_t atlas_lpoint =  	y0_s_base+(b_w - tile_safe_w_sub);	
			const uint32_t atlas_y_mid_point = ((y0 + (b_h >> 1U)) * a_w)+(b_w>>1U);
			const uint32_t atlas_dpoint = (( y0 + (b_h-tile_safe_h_sub )) *a_w );
			const uint32_t atlas_d2point = atlas_dpoint + (b_w-tile_safe_w_sub);
			uint32_t x0 = atlas_x0 ,x1 = atlas_x1;

			register __m128i mrep0 = _mm_set1_epi8(tile_spoint_pixc);
			register __m128i mrep1 = _mm_set1_epi8(tile_lpoint_pixc);
			register __m128i mrep2 = _mm_set1_epi8(tile_dpoint_pixc);
			register __m128i mrep3 = _mm_set1_epi8(tile_d2point_pixc);
			register __m128i mrep4 = _mm_set1_epi8(tile_mid_point_pixc);

			/*Optimization pass*/
			{		
				register const uint8_t* patlas_pixels = &atlas_pixels[y0_s_base];
				uint16_t r = 0;
				__m128i ld;

				if ((x0 + 16U) <= x1) {
					do {
						_mm_prefetch((const char*)&patlas_pixels[x1 - (512 - 128)] ,_MM_HINT_NTA);
						ld = _mm_loadu_si128((const __m128i*)(const char*)&patlas_pixels[(x1 - 16U)]);
						r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep1,ld));
						if (r)
							break;
						x1 -= 16U ;
					} while ( (x0 + 16U) <= x1);
				}

				if ((x0 + 16U) <= x1) {
					do {
						_mm_prefetch((const char*)&patlas_pixels[x0 + (512 - 128)] ,_MM_HINT_NTA);
						ld = _mm_loadu_si128((const __m128i*)(const char*)&patlas_pixels[x0]);
						r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep1,ld));
						if (r)
							break;
						x0 += 16U ;
					} while ((x0 + 16U) <= x1);
				}
			}


			//mrep0 = _mm_loadu_si128((const __m128i*)(const char*)tile_pixels);
			//mrep1 = _mm_loadu_si128((const __m128i*)(const char*)&tile_pixels[b_w - 1U]);
			//mrep4 = _mm_loadu_si128((const __m128i*)(const char*)&tile_pixels[tile_mid_point]);
			for (;x0 < x1;++x0) {
				if ((x0 + b_w) > a_w) { break; }
				/*Test edges*/
				{	
					/*Jump to next missmatch */
					_mm_prefetch((const char*)&atlas_pixels[y0_s_base + x0 + (512 - 128)] ,_MM_HINT_NTA);
		    		__m128i  ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[y0_s_base+x0]);
					uint16_t r,j = 0U;

					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep0,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}

					ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[atlas_lpoint + x0]);
					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep1,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}

					ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[atlas_d2point + x0]);
					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep3,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}

					ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[atlas_y_mid_point + x0]);
					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep4,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}
				}

				found = false;
					
				/*Actual cmp*/
				uint32_t y1,y1_h = b_h,y1_base=0U,y0_base = y0_s_base;
				
				for ( y1 = 0U;y1 < y1_h;++y1,y1_base+=b_w,y0_base += a_w) {
					register const uint8_t* pa = &atlas_pixels[y0_base + x0];
					register const uint8_t* pb = &tile_pixels[y1_base];
					uint32_t x2 = ln_blocks;

					found = true;

					  { 
		 

						/*
							2bit mask table :
							11 : pa & pb aligned
							10 : pa aligned / pb unaligned
							01 : pa unaligned / pb aligned
							00 : both unaligned
						*/

						const uint32_t cmp_jlut_mask = ((0U==((ptrdiff_t)pa & 15U)) << 1U) | (0U==((ptrdiff_t)pb & 15U));
		
						switch (cmp_jlut_mask) {
							case 0x03: {	
								do { 
									_mm_prefetch((const char*)pa + (512 - 128),_MM_HINT_NTA);
									//_mm_prefetch((const char*)&pb[addr] + (512 - 128),_MM_HINT_NTA);
								 	__m128i p0 = _mm_load_si128((__m128i*)&pa[0]);
									__m128i p1 = _mm_load_si128((__m128i*)&pa[16]);
								 	__m128i p2 = _mm_load_si128((__m128i*)&pb[0]);
									__m128i p3 = _mm_load_si128((__m128i*)&pb[16]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									
									mcmp = _mm_cmpeq_epi8(p1,p3);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 32; pb += 32; --x2;	
								} while (x2 > 0U);
								break;
							}
						
							case 0x02: { 
								do { 
									_mm_prefetch((const char*)pa + (512 - 128),_MM_HINT_NTA);
									//_mm_prefetch((const char*)&pb[addr] + (512 - 128),_MM_HINT_NTA);
								 	__m128i p0 = _mm_load_si128((__m128i*)&pa[0]);
									__m128i p1 = _mm_load_si128((__m128i*)&pa[16]);
								 	__m128i p2 = _mm_loadu_si128((__m128i*)&pb[0]);
									__m128i p3 = _mm_loadu_si128((__m128i*)&pb[16]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									
									mcmp = _mm_cmpeq_epi8(p1,p3);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 32; pb += 32; --x2;	
								} while (x2 > 0U);
								break;
							}
							
							case 0x01: { 
								do { 
									_mm_prefetch((const char*)pa + (512 - 128),_MM_HINT_NTA);
									//_mm_prefetch((const char*)&pb[addr] + (512 - 128),_MM_HINT_NTA);
								 	__m128i p0 = _mm_loadu_si128((__m128i*)&pa[0]);
									__m128i p1 = _mm_loadu_si128((__m128i*)&pa[16]);
								 	__m128i p2 = _mm_load_si128((__m128i*)&pb[0]);
									__m128i p3 = _mm_load_si128((__m128i*)&pb[16]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									
									mcmp = _mm_cmpeq_epi8(p1,p3);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 32; pb += 32; --x2;	
								} while (x2 > 0U);
								break;
							}

							default: { 
								do { 
									_mm_prefetch((const char*)pa + (512 - 128),_MM_HINT_NTA);
									//_mm_prefetch((const char*)&pb[addr] + (512 - 128),_MM_HINT_NTA);
								 	__m128i p0 = _mm_loadu_si128((__m128i*)&pa[0]);
									__m128i p1 = _mm_loadu_si128((__m128i*)&pa[16]);
								 	__m128i p2 = _mm_loadu_si128((__m128i*)&pb[0]);
									__m128i p3 = _mm_loadu_si128((__m128i*)&pb[16]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									
									mcmp = _mm_cmpeq_epi8(p1,p3);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 32; pb += 32; --x2;	
								} while (x2 > 0U);
								break;
							}
						}
 
						if (!found)
							break;

					}//X2 >= 32U
				}

				if (found) {
					result_t res;
					res.pattern = args->pattern_id;
					res.px = x0;
					res.py = y0;
					output.push_back(res);
					x0 += b_w-tile_safe_w_sub;
				} 
			}
		}
	}
	mt_entry_point_epilogue(NULL);
}

static mt_entry_point_result_t mt_solve_entry_point_algo_block16( mt_entry_point_stack_args ) {
	mt_solve_entry_point_algo_args_t* args;

	mt_entry_point_prologue(args,mt_solve_entry_point_algo_args_t); 
	{
		const uint8_t* atlas_pixels = args->atlas_pixels;
		const uint8_t* tile_pixels = args->tile_pixels;
		const uint32_t atlas_y1 = args->atlas_y1,atlas_x1 = args->atlas_x1,atlas_x0 = args->atlas_x0;
		uint32_t y0 = args->atlas_y0;
		const uint32_t a_w=args->atlas_w,b_w=args->tile_w,a_h=args->atlas_h,b_h=args->tile_h;

		/*Precalculate tile pattern edges*/
		const uint32_t tile_safe_w_sub = (b_w != 0U),tile_safe_h_sub = (b_h != 0U);
		const uint32_t tile_mid_point = ((b_h >> 1U) *b_w) + (b_w >> 1U);
		const uint32_t tile_mid_point_0 = ((b_h >> 1U) * b_w) + (b_w-tile_safe_w_sub);
		const uint32_t tile_mid_point_1 = (b_h >> 1U);
		const uint8_t tile_mid_point_pixc = tile_pixels[tile_mid_point];
		const uint8_t tile_spoint_pixc = tile_pixels[0U];
		const uint8_t tile_lpoint_pixc = tile_pixels[b_w - tile_safe_w_sub];
		const uint8_t tile_d2point_pixc = tile_pixels[(b_h-tile_safe_h_sub )*b_w + (b_w-tile_safe_w_sub)];
		const uint8_t tile_dpoint_pixc = tile_pixels[(b_h-tile_safe_h_sub )];
		uint32_t y0_s_base =  y0 * a_w;
		
		bool found;

		/*XXX : NOTE It is expected to go out of thread tile bounds(by + tile->h) if(&only IF) a possible match is found  */
 
		const uint32_t ln_blocks = args->line_blocks;
		const uint32_t pattern_id = args->pattern_id;
		std::vector<result_t>& output = args->matches;

		 for (;y0 < atlas_y1;++y0,y0_s_base += a_w) {
			if (((y0 + b_h) ) > a_h) { /*Out of bounds*/
				break; 
			}

			/*Precalculate atlas pattern edges*/
			const uint32_t atlas_lpoint =  	y0_s_base+(b_w - tile_safe_w_sub);	
			const uint32_t atlas_y_mid_point = ((y0 + (b_h >> 1U)) * a_w)+(b_w>>1U);
			const uint32_t atlas_dpoint = (( y0 + (b_h-tile_safe_h_sub )) *a_w );
			const uint32_t atlas_d2point = atlas_dpoint + (b_w-tile_safe_w_sub);
			uint32_t x0 = atlas_x0 ,x1 = atlas_x1;

			register __m128i mrep0 = _mm_set1_epi8(tile_spoint_pixc);
			register __m128i mrep1 = _mm_set1_epi8(tile_lpoint_pixc);
			register __m128i mrep2 = _mm_set1_epi8(tile_dpoint_pixc);
			register __m128i mrep3 = _mm_set1_epi8(tile_d2point_pixc);
			register __m128i mrep4 = _mm_set1_epi8(tile_mid_point_pixc);

			/*Optimization pass*/
			{		
				register const uint8_t* patlas_pixels = &atlas_pixels[y0_s_base];
				uint16_t r = 0;
				__m128i ld;

				if ((x0 + 16U) <= x1) {
					do {
						_mm_prefetch((const char*)&patlas_pixels[x1 - (512 - 128)] ,_MM_HINT_NTA);
						ld = _mm_loadu_si128((const __m128i*)(const char*)&patlas_pixels[(x1 - 16U)]);
						r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep1,ld));
						if (r)
							break;
						x1 -= 16U ;
					} while ( (x0 + 16U) <= x1);
				}

				if ((x0 + 16U) <= x1) {
					do {
						_mm_prefetch((const char*)&patlas_pixels[x0 + (512 - 128)] ,_MM_HINT_NTA);
						ld = _mm_loadu_si128((const __m128i*)(const char*)&patlas_pixels[x0]);
						r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep1,ld));
						if (r)
							break;
						x0 += 16U ;
					} while ((x0 + 16U) <= x1);
				}
			}


			//mrep0 = _mm_loadu_si128((const __m128i*)(const char*)tile_pixels);
			//mrep1 = _mm_loadu_si128((const __m128i*)(const char*)&tile_pixels[b_w - 1U]);
			//mrep4 = _mm_loadu_si128((const __m128i*)(const char*)&tile_pixels[tile_mid_point]);
			for (;x0 < x1;++x0) {
				if ((x0 + b_w) > a_w) { break; }

				/*Test edges*/
					/*Jump to next missmatch */
					_mm_prefetch((const char*)&atlas_pixels[y0_s_base + x0 + (512 - 128)] ,_MM_HINT_NTA);
		    		__m128i  ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[y0_s_base+x0]);
					uint16_t r,j = 0U;

					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep0,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}

					ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[atlas_lpoint + x0]);
					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep1,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}

					ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[atlas_d2point + x0]);
					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep3,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}

					ld = _mm_loadu_si128((const __m128i*)(const char*)&atlas_pixels[atlas_y_mid_point + x0]);
					r = (uint16_t)_mm_movemask_epi8(_mm_cmpeq_epi8(mrep4,ld));
					if (g_sse_msk_jmp_neq_lut[r] != 0) {
						x0 += g_sse_msk_jmp_neq_lut[r] - 1U;
						continue;
					}


				found = false;
					
				/*Actual cmp*/
				uint32_t y1,y1_h = b_h,y1_base=0U,y0_base = y0_s_base;
				
				for ( y1 = 0U;y1 < y1_h;++y1,y1_base+=b_w,y0_base += a_w) {
					register const uint8_t* pa = &atlas_pixels[y0_base + x0];
					register const uint8_t* pb = &tile_pixels[y1_base];
					uint32_t x2 = ln_blocks ;

					found = true;

					{ 

						/*
							2bit mask table :
							11 : pa & pb aligned
							10 : pa aligned / pb unaligned
							01 : pa unaligned / pb aligned
							00 : both unaligned
						*/
						const uint32_t cmp_jlut_mask = ((0U==((ptrdiff_t)pa & 15U)) << 1U) | (0U==((ptrdiff_t)pb & 15U));
						switch (cmp_jlut_mask) {
							case 0x03: {	
								do { 
								 	__m128i p0 = _mm_load_si128((__m128i*)&pa[0]);
								 	__m128i p2 = _mm_load_si128((__m128i*)&pb[0]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 16; pb += 16; --x2;
								} while (x2 > 0);
								break;
							}
						
							case 0x02: { 
								do { 
								 	__m128i p0 = _mm_load_si128((__m128i*)&pa[0]);
								 	__m128i p2 = _mm_loadu_si128((__m128i*)&pb[0]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 16; pb += 16; --x2;
								} while (x2 > 0);
								break;
							}
							
							case 0x01: { 
								do { 
								 	__m128i p0 = _mm_loadu_si128((__m128i*)&pa[0]);
								 	__m128i p2 = _mm_load_si128((__m128i*)&pb[0]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 16; pb += 16; --x2;
								} while (x2 > 0);
								break;
							}

							default: { 
								do { 
								 	__m128i p0 = _mm_loadu_si128((__m128i*)&pa[0]);
								 	__m128i p2 = _mm_loadu_si128((__m128i*)&pb[0]);
									__m128i mcmp = _mm_cmpeq_epi8(p0,p2);
									if ((uint16_t)_mm_movemask_epi8(mcmp) != 0xffffU) {
										found = false; 
										break; 
									}
									pa += 16; pb += 16; --x2;
								} while (x2 > 0);
								break;
							}
						}
 
						if (!found)
							break;

					}//X2 >= 16U
				}

				if (found) {
					result_t res;
					res.pattern = pattern_id;
					res.px = x0;
					res.py = y0;
					output.push_back(res);
					x0 += b_w-tile_safe_w_sub;
				} 
			}
		}
	}
	mt_entry_point_epilogue(NULL);
}
#else


/*

static inline uint8_t m16_2_m8(__mmask16 in) {
	in = ((in >> 1U) | in ) & 0x3333U;
	in = ((in >> 2U) | in) & 0xF0FU;
	return ((in >> 4U) | in) & 0xFFU;
}

static inline __attribute__((always_inline)) __m512 _mm512_loadu_ps(float* addr) {
	register __m512 ret;

	asm volatile(".set push,1\n"
				".set noreorder,1\n"
				".text\n"
				"vmovups	%0,0(%1)\n"
				".set pop,1\n" : : "r"(&ret),"r"(addr) : );

	return ret;
}

static inline  __attribute__((always_inline)) __m512d _mm512_loadu_pd(double* addr) {
	register __m512d ret;

	asm volatile(".set push,1\n"
				".set noreorder,1\n"
				".text\n"
				"vmovupd	%0,0(%1)\n"
				".set pop,1\n" : : "r"(&ret),"r"(addr) : );

	return ret;
}
*/
/*XEON PHI*/
static mt_entry_point_result_t mt_solve_entry_point_algo_block64( mt_entry_point_stack_args ) {
	mt_solve_entry_point_algo_args_t* args;

	mt_entry_point_prologue(args,mt_solve_entry_point_algo_args_t); 
	{
		const uint8_t* atlas_pixels = args->atlas_pixels;
		const uint8_t* tile_pixels = args->tile_pixels;
		const uint32_t atlas_y1 = args->atlas_y1,atlas_x1 = args->atlas_x1,atlas_x0 = args->atlas_x0;
		uint32_t y0 = args->atlas_y0;
		const uint32_t a_w=args->atlas_w,b_w=args->tile_w,a_h=args->atlas_h,b_h=args->tile_h;

		/*Precalculate tile pattern edges*/
		const uint32_t tile_safe_w_sub = (b_w != 0U),tile_safe_h_sub = (b_h != 0U);
		const uint32_t tile_mid_point = ((b_h >> 1U) *b_w) + (b_w >> 1U);
		const uint32_t tile_mid_point_0 = ((b_h >> 1U) * b_w) + (b_w-tile_safe_w_sub);
		const uint32_t tile_mid_point_1 = (b_h >> 1U);
		const uint8_t tile_mid_point_pixc = tile_pixels[tile_mid_point];
		const uint8_t tile_spoint_pixc = tile_pixels[0U];
		const uint8_t tile_lpoint_pixc = tile_pixels[b_w - tile_safe_w_sub];
		const uint8_t tile_d2point_pixc = tile_pixels[(b_h-tile_safe_h_sub )*b_w + (b_w-tile_safe_w_sub)];
		const uint8_t tile_dpoint_pixc = tile_pixels[(b_h-tile_safe_h_sub )];
		uint32_t y0_s_base =  y0 * a_w;
		
		bool found;

		/*XXX : NOTE It is expected to go out of thread tile bounds(by + tile->h) if(&only IF) a possible match is found  */
 
		const uint32_t ln_blocks = args->line_blocks;
		const uint32_t pattern_id = args->pattern_id;
		std::vector<result_t>& output = args->matches;

		 for (;y0 < atlas_y1;++y0,y0_s_base += a_w) {
			if (((y0 + b_h) ) > a_h) { /*Out of bounds*/
				break; 
			}

			/*Precalculate atlas pattern edges*/
			const uint32_t atlas_lpoint =  	y0_s_base+(b_w - tile_safe_w_sub);	
			const uint32_t atlas_y_mid_point = ((y0 + (b_h >> 1U)) * a_w)+(b_w>>1U);
			const uint32_t atlas_dpoint = (( y0 + (b_h-tile_safe_h_sub )) *a_w );
			const uint32_t atlas_d2point = atlas_dpoint + (b_w-tile_safe_w_sub);
			uint32_t x0 = atlas_x0 ,x1 = atlas_x1;

			for (;x0 < x1;++x0) {
				if ((x0 + b_w) > a_w) { break; }


				found = false;
					
				/*Actual cmp*/
				uint32_t y1,y1_h = b_h,y1_base=0U,y0_base = y0_s_base;
				
				for ( y1 = 0U;y1 < y1_h;++y1,y1_base+=b_w,y0_base += a_w) {
					const uint8_t* pa = &atlas_pixels[y0_base + x0];
					const uint8_t* pb = &tile_pixels[y1_base];
					uint32_t x2 = ln_blocks ;

					found = true;
					const __m512d p3 = _mm512_setzero_pd();
					do { 
						__m512d p0 = _mm512_loadunpacklo_pd(p3,(void*)pa); p0 = _mm512_loadunpackhi_pd(p0,(void*)&pa[64]);
						__m512d p2 = _mm512_loadunpacklo_pd(p3,(void*)pb); p2 = _mm512_loadunpackhi_pd(p2,(void*)&pb[64]);

						//if (0== ((_mm512_cmpeq_pd_mask(p0,p2) & (1<<8)) != 0) ) { 
						if ((_mm512_cmpeq_pd_mask(p0,p2) >> 8) != 0xff) {
							found = false; 
							break; 
						}
						pa += 64; pb += 64; --x2;
					} while (x2 > 0);

						if (!found)
							break; 
				}

				if (found) {
					result_t res;
					res.pattern = pattern_id;
					res.px = x0;
					res.py = y0;
					output.push_back(res);
					x0 += b_w-tile_safe_w_sub;
				} 
			}
		}
	}
	mt_entry_point_epilogue(NULL);
}
#endif

static mt_entry_point_result_t mt_solve_entry_point_algo_small_block( mt_entry_point_stack_args ) {
	mt_solve_entry_point_algo_args_t* args;

	mt_entry_point_prologue(args,mt_solve_entry_point_algo_args_t); 
	{
		const uint8_t* atlas_pixels = args->atlas_pixels;
		const uint8_t* tile_pixels = args->tile_pixels;
		const uint32_t atlas_y1 = args->atlas_y1,atlas_x1 = args->atlas_x1,atlas_x0 = args->atlas_x0;
		uint32_t y0 = args->atlas_y0;
		const uint32_t a_w=args->atlas_w,b_w=args->tile_w,a_h=args->atlas_h,b_h=args->tile_h;

		/*Precalculate tile pattern edges*/
		const uint32_t tile_safe_w_sub = (b_w != 0U),tile_safe_h_sub = (b_h != 0U);
		const uint32_t tile_mid_point = ((b_h >> 1U) *b_w) + (b_w >> 1U);
		const uint32_t tile_mid_point_0 = ((b_h >> 1U) * b_w) + (b_w-tile_safe_w_sub);
		const uint32_t tile_mid_point_1 = (b_h >> 1U);
		const uint8_t tile_mid_point_pixc = tile_pixels[tile_mid_point];
		const uint8_t tile_spoint_pixc = tile_pixels[0U];
		const uint8_t tile_lpoint_pixc = tile_pixels[b_w - tile_safe_w_sub];
		const uint8_t tile_d2point_pixc = tile_pixels[(b_h-tile_safe_h_sub )*b_w + (b_w-tile_safe_w_sub)];
		const uint8_t tile_dpoint_pixc = tile_pixels[(b_h-tile_safe_h_sub )];
		const uint32_t pattern_id = args->pattern_id;
		uint32_t y0_s_base =  y0 * a_w;
		
		bool found;

		/*XXX : NOTE It is expected to go out of thread tile bounds(by + tile->h) if(&only IF) a possible match is found  */
 		std::vector<result_t>& output = args->matches;

		 for (;y0 < atlas_y1;++y0,y0_s_base += a_w) {
			if (((y0 + b_h) ) > a_h) { /*Out of bounds*/
				break; 
			}

			/*Precalculate atlas pattern edges*/
			const uint32_t atlas_lpoint =  	y0_s_base+(b_w - tile_safe_w_sub);	
			const uint32_t atlas_y_mid_point = ((y0 + (b_h >> 1U)) * a_w)+(b_w>>1U);
			const uint32_t atlas_dpoint = (( y0 + (b_h-tile_safe_h_sub )) *a_w );
			const uint32_t atlas_d2point = atlas_dpoint + (b_w-tile_safe_w_sub);
			uint32_t x0 = atlas_x0 ,x1 = atlas_x1;

			{
				/*Optimization pass*/
				register const uint8_t* patlas_pixels;

				/*Shrink base offset if possible*/
				patlas_pixels = &atlas_pixels[y0_s_base];

				if (*(patlas_pixels) != tile_spoint_pixc) {  
					for (;x0 < x1;++x0) {
						if (*(patlas_pixels++) == tile_spoint_pixc) {  
							break;
						}
					}
				}

				patlas_pixels = &atlas_pixels[y0_s_base + x1];
				if (*(patlas_pixels - 1U) != tile_lpoint_pixc) {
					patlas_pixels -= x1 > x0;
					x1 -= x1 > x0;
					for (;x1 > x0;--x1) {	
						if (*(patlas_pixels--) == tile_lpoint_pixc) {
							break;
						}
					}
				}
			}

			for (;x0 < x1;++x0) {
				if ((x0 + b_w) > a_w) { break; }
				/*Test edges*/
 
				if (atlas_pixels[y0_s_base+x0] != tile_spoint_pixc) {
					continue;
				} else if (atlas_pixels[atlas_lpoint + x0] != tile_lpoint_pixc) {
					continue;	
				}/* else if (atlas_pixels[atlas_dpoint+x0] != tile_dpoint_pixc) {
					continue;	
				}*/ else if (atlas_pixels[atlas_d2point + x0] != tile_d2point_pixc) {
					continue;	
				} else if (atlas_pixels[atlas_y_mid_point + x0] != tile_mid_point_pixc) {
					continue;	
				}		

				found = false;
					
				/*Actual cmp*/
				uint32_t y1,y1_h = b_h,y0_base = y0_s_base;
				register const uint8_t* pb = tile_pixels;
				for ( y1 = 0U;y1 < y1_h;++y1,y0_base += a_w) {
					register const uint8_t* pa = &atlas_pixels[y0_base + x0];
					register uint32_t x2 = b_w ;

					found = true;
					do {
						if (*(pa++) != *(pb++)) {
							found = false;
							break;
						} 	
					} while (--x2 > 0U);
		
					if (!found)
						break;
				}

				if (found) {
					result_t res;
					res.pattern = pattern_id;
					res.px = x0;
					res.py = y0;
					output.push_back(res);
					x0 += b_w-tile_safe_w_sub;
				} 
			}
		}
	}
	mt_entry_point_epilogue(NULL);
}
 


static mt_entry_point_result_t mt_solve_entry_point_algo_confirmation_pass( mt_entry_point_stack_args ) {
	mt_solve_entry_point_algo_args_t* args;

	mt_entry_point_prologue(args,mt_solve_entry_point_algo_args_t); 
	{
		if (0 == args->matches_to_confirm) {
			printf("args->matches_to_confirm == 0\n");
			assert(0);
		}

		const uint8_t* atlas_pixels = args->atlas_pixels;
		const uint8_t* tile_pixels = args->tile_pixels;
		const uint32_t a_w=args->atlas_w,b_w=args->tile_w, b_h=args->tile_h;
		const uint32_t rel_x0 = args->block_size * args->line_blocks;
		const uint32_t rel_x1 = rel_x0 + (args->tile_w - rel_x0);
		register const result_t* input_head = &args->matches_to_confirm[args->input_list_start];
		register const result_t* input_tail = &args->matches_to_confirm[args->input_list_end];
		const uint32_t run_x = rel_x1 - rel_x0;
		const uint32_t pattern_id = args->pattern_id;

		bool found;

		std::vector<result_t>& output = args->matches;
		if (run_x > 0) {
			for (;input_head < input_tail;++input_head) {
				const uint32_t px = input_head->px + rel_x0;
				const uint32_t py = input_head->py ;

				found = false;

				uint32_t y1,y1_h = b_h,y1_base = 0,y0_base = py * a_w;
		
				for ( y1 = 0U;y1 < y1_h;++y1,y0_base += a_w,y1_base += b_w) {
					register const uint8_t* pa = &atlas_pixels[y0_base + px];
					register const uint8_t* pb = &tile_pixels[y1_base + rel_x0];
					register uint32_t x2 = run_x;

					found = true;
					do {
						if (*(pa++) != *(pb++)) {
							found = false;
							break;
						} 	
					} while (--x2 > 0U);
		
					if (!found)
						break;
				}

				if (found) {
					result_t res;
					res.pattern = pattern_id;
					res.px = px - rel_x0;
					res.py = py;
					output.push_back(res);
				}
			}
		}
	}
	mt_entry_point_epilogue(NULL);
}

static bool solve_mt2(const parameters_t& parameters,std::vector<result_t>& result_list) {
	pixel_chunk_t* xform;
	pixel_chunk_t* atlas = pixel_chunk_new_mapped(parameters.main_image_name);
	result_t result;

	if (!atlas) {
		printf("solve_mt : FAILED - Unable to load %s\n",parameters.main_image_name.c_str());
		return false;
	}

	const uint32_t avail_threads = mt_get_thread_contexts();
	std::vector<result_t> conf_list;
	std::vector<mt_solve_entry_point_algo_args_t> worker_args;
	std::vector<mt_extent_t> extent;
	std::vector<mt_extent_t> extent2;

	mt_calculate_extent(extent,atlas->h,avail_threads);

	const uint32_t active_threads = extent.size();
	
	worker_args.reserve(active_threads);

	for (uint32_t i = 0U;i < active_threads;++i) {
		worker_args.push_back(mt_solve_entry_point_algo_args_t());
		worker_args[i].atlas_pixels = atlas->pixels;
		worker_args[i].atlas_w = atlas->w;
		worker_args[i].atlas_h = atlas->h;
		worker_args[i].atlas_x0 = 0U;
		worker_args[i].atlas_x1 = atlas->w;
		worker_args[i].atlas_y0 = extent[i].s0;
		worker_args[i].atlas_y1 = extent[i].s1;
		worker_args[i].thread_id = i;
	}
 
	bool ret = true;
 
	result_list.clear();
	result_list.reserve(2048);
	conf_list.reserve(1024);

	for (const std::string& temp_name : parameters.template_names) {
		const int32_t template_id = atoi(temp_name.substr(0, 3).c_str());	
		pixel_chunk_t* tile = pixel_chunk_new_mapped(temp_name),*scaled = tile;

		if (!tile) {
			printf("Unable to load tile %s\n",temp_name.c_str());
			ret = false;		
			break;
		}

		for (uint32_t i = 0U;i < active_threads;++i)
			worker_args[i].pattern_id = template_id;

		for (uint32_t s = 1U;s <= parameters.max_scale;++s) {
			for (uint32_t stub_index = 0U;0 != g_xofrm_stubs[stub_index];++stub_index) {
				xform = g_xofrm_stubs[stub_index](scaled);

				bool do_block_pass;
				bool do_confirmation_pass;
				uint32_t block_size = 0,block_nb = 0;

#ifdef __MIC__BUILD__
				do_block_pass = false;//(bool)(xform->w >= 64);
#else
				do_block_pass = (bool)(xform->w >= 16);
#endif
				if (!do_block_pass)
					do_confirmation_pass = false;
				else {
#ifdef __MIC__BUILD__
					block_size = 64;
#else
					if (xform->w >= 32)
						block_size = 32;
					else
						block_size = 16;
#endif
					if (0 == (xform->w % block_size))
						do_confirmation_pass = false;
					else
						do_confirmation_pass = true;

					block_nb = xform->w / block_size;

					if ((xform->w - (block_size * block_nb)) == 0)
						do_confirmation_pass = false;
				}

				for (uint32_t i = 0U;i < active_threads;++i) {
					worker_args[i].tile_pixels = xform->pixels;
					worker_args[i].tile_w = xform->w;
					worker_args[i].tile_h = xform->h;
					worker_args[i].line_blocks = block_nb;
					worker_args[i].block_size = block_size;
 
					if (!do_block_pass)
						mt_call_thread(mt_solve_entry_point_algo_small_block,(void*)&worker_args[i],i); 
					else {
#ifdef __MIC__BUILD__
						mt_call_thread(mt_solve_entry_point_algo_block64,(void*)&worker_args[i],i);	
#else
						switch (block_size) {
							case 16:
								mt_call_thread(mt_solve_entry_point_algo_block16,(void*)&worker_args[i],i);	
							break;
							case 32:
								mt_call_thread(mt_solve_entry_point_algo_block32,(void*)&worker_args[i],i);	
							break;
							default : printf("MT_OP FAILED! Unknown block size %u\n",block_size); assert(0); break;
						}
#endif
					}	

				}
				mt_wait_threads( active_threads );

				//Confirm
				if (do_confirmation_pass) {
					extent2.clear();
					conf_list.clear();

					for (uint32_t i = 0U;i < active_threads;++i ) {
						const std::vector<result_t>& res = worker_args[i].matches;
						for (uint32_t j = 0U,k = (uint32_t)res.size();j < k;++j)
							conf_list.push_back(res[j]);

						worker_args[i].matches.clear();
					}

					if (!conf_list.empty()) {
						mt_calculate_extent(extent2,conf_list.size(),avail_threads);

						const uint32_t th_cnt = extent2.size();

						for (uint32_t i = 0U;i < th_cnt;++i ) {
							worker_args[i].input_list_start = extent2[i].s0;
							worker_args[i].input_list_end = extent2[i].s1;
							worker_args[i].matches_to_confirm = &conf_list[0];
							mt_call_thread(mt_solve_entry_point_algo_confirmation_pass,(void*)&worker_args[i],i); 
						}

						mt_wait_threads( th_cnt );
					}
				}
 

				if (xform != scaled)
					pixel_chunk_delete(xform);

				//Append valid results
				for (uint32_t i = 0U;i < avail_threads;++i ) {
					const std::vector<result_t>& res = worker_args[i].matches;
		
					for (uint32_t j = 0U,k = (uint32_t)res.size();j < k;++j)
						result_list.push_back(res[j]);

					worker_args[i].matches.clear();
				}
			} //for xforms


			if ((s + 1U) <= parameters.max_scale) {
				//profiler_profile_me_ex("scale");

				if ((tile->w * (s + 1U)) > atlas->w) {
					//printf("TileW out of bounds %u\n",tile->w*s);
					break;
				} else if ((tile->h * (s + 1U)) > atlas->h) {
					//printf("TileH out of bounds %u\n",tile->h*s);
					break;
				}
				if ( (s != 1U) && (tile != scaled)) {
					pixel_chunk_delete(scaled);
				}
				const uint32_t iters = (tile->w * (s + 1U)) + (tile->h * (s + 1U)) ;

				scaled =  (iters >= 800) ? pixel_chunk_upscale_mt(tile,s + 1U) : pixel_chunk_upscale(tile,s + 1U);
				
				//pixel_chunk_dump(scaled,"scale.tga");
				if (!scaled) {
					printf("Unable to scale tile %s - out of memory!\n",temp_name.c_str());
					ret = false;		
					break;
				}
			}  
		}
	 	if (scaled != tile) {
			pixel_chunk_delete(scaled);
		}
		pixel_chunk_delete(tile);

		if (!ret) {
			break;
		}
	}

	pixel_chunk_delete(atlas);

	return ret;
}

bool solve_mt(const parameters_t& parameters,std::vector<result_t>& result_list) {
	return solve_mt2(parameters,result_list);
}
 
static void xform_block_90deg_ccw(const uint8_t* in,uint8_t* out,uint32_t w,uint32_t h) {
	const uint32_t surf_len = w * h;
	const uint8_t* ep;
	const uint8_t* sp;
	register const uint8_t* p_in;
	register uint8_t* p_out;
	register uint32_t y;

    ep = &in[surf_len];
    p_out = &out[surf_len];
    for (sp = ep - w;sp < ep;++sp) {
        p_in = sp;
        for (y = h;y > 0;--y,p_in -= w) {
            *(--p_out) = *p_in;
		}
    }
}

static void xform_block_90deg_cw(const uint8_t* in,uint8_t* out,uint32_t w,uint32_t h) {
    const uint8_t* ep;
    const uint8_t* sp;
	register const uint8_t* p_in;
	register uint8_t* p_out;
    register uint32_t y;

	p_out = out;
    ep = &in[w*h];
    for (sp = ep - w;sp < ep;++sp) {
        p_in = sp;
        for (y = h;y > 0U;--y,p_in -= w) {
            *(p_out++) = *p_in;
		}
    }
}

static pixel_chunk_t* xform_identity(pixel_chunk_t* in_chunk) {
	return in_chunk;
}

static pixel_chunk_t* xform_180deg(pixel_chunk_t* in_chunk) {
	uint8_t* e;
	pixel_chunk_t* chunk;

	if (!in_chunk) {
		return 0;
	}
	
	chunk = new pixel_chunk_t;
	if (!chunk) {
		printf("xform_180deg Out of memory!\n");
		return 0;
	}

	memcpy(chunk,in_chunk,sizeof(pixel_chunk_t));
	e = (uint8_t*)heap_new(chunk->w*chunk->h);

	if (!e) {
		printf("xform_180deg Out of memory!\n");
		delete chunk;
		return 0;
	}

	uint8_t* pixels = e;
	const uint32_t w = chunk->w,h = chunk->h;
	uint32_t base0 = 0U,base1 = w * (h - 1U);

	/*XXX faster than xform_180deg_no_alloc() because of 1w+1r op vs 2w+2r ops*/
	for (uint32_t y = 0U;y < h;++y,base0 += w,base1 -= w) {
		register const uint8_t* rd = (const uint8_t*)&chunk->pixels[base0 ];
		register uint8_t* wr = (uint8_t*)&pixels[base1 + w];
		for (register uint32_t x = 0U,x1 = w;x < x1;++x) {
			*(--wr) = *(rd++);
		}
	}

	chunk->pixels = e;

	return chunk;
}



static pixel_chunk_t* xform_90deg(pixel_chunk_t* in_chunk) {
	uint8_t* e;
	pixel_chunk_t* chunk;

	if (!in_chunk) {
		return 0;
	}
	
	chunk = new pixel_chunk_t;
	if (!chunk) {
		printf("xform_90deg Out of memory!\n");
		return 0;
	}

	memcpy(chunk,in_chunk,sizeof(pixel_chunk_t));
	e = (uint8_t*)heap_new(chunk->w*chunk->h);

	if (!e) {
		printf("xform_90deg Out of memory!\n");
		delete chunk;
		return 0;
	}


	chunk->pixels=e;
 
	xform_block_90deg_cw(in_chunk->pixels, chunk->pixels, chunk->w, chunk->h);


	{
		uint32_t tmp = chunk->w;
		chunk->w = chunk->h;
		chunk->h = tmp;
	}

	return chunk;
}



static pixel_chunk_t* xform_270deg(pixel_chunk_t* in_chunk) {
	uint8_t* e;
	pixel_chunk_t* chunk;

	if (!in_chunk) {
		return 0;
	}
	
	chunk = new pixel_chunk_t;
	if (!chunk) {
		printf("xform_270deg Out of memory!\n");
		return 0;
	}

	memcpy(chunk,in_chunk,sizeof(pixel_chunk_t));
	e = (uint8_t*)heap_new(chunk->w*chunk->h);

	if (!e) {
		printf("xform_270deg Out of memory!\n");
		delete chunk;
		return 0;
	}


	chunk->pixels = e;
	xform_block_90deg_ccw(in_chunk->pixels, chunk->pixels,chunk->w,chunk->h);
	{
		uint32_t tmp = chunk->w;
		chunk->w = chunk->h;
		chunk->h = tmp;
	}

	return chunk;
}




