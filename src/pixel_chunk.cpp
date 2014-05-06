/*
	1/8BIT(grayscale) pixel chunk
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#include "types.hpp"
#include "pixel_chunk.hpp"
#include "bgr2gu8.h"
#include "gu8_to_mono.h"
#include "mt.hpp"
#include "aligned_heap.hpp"

struct heap_align_attr mt_bgr_to_gu8_args_t {
	uint32_t y0,y1,bpp,padding;
	uint32_t width,height;
	const uint8_t* rd_data;
	uint8_t* wr_data;
};

struct heap_align_attr mt_upscaler_args_t {
	const uint8_t* in_data;
	uint8_t* out_data;
	uint32_t w,h,ow;
	uint32_t y0,y1;
	f64_t rw;
	f64_t rh;
};

template <typename base_t>
static inline base_t align_offset(const base_t in,const base_t alignment) {
	const base_t m = alignment - (base_t)1;
	return (in + m) & (~m);
}

template <typename base_t>
static inline f64_t deg_to_rad(const base_t angle) {
	return (f64_t)angle * (3.14159265359 / 180.0);
}
 
/*
	The parallel version of the upscaler
*/

static mt_entry_point_result_t upscaler_entry_point( mt_entry_point_stack_args ) {
	mt_upscaler_args_t* args;

	mt_entry_point_prologue(args,mt_upscaler_args_t); 

	uint32_t base = args->y0 * args->w;
	const uint8_t* in = args->in_data;
	uint8_t* out = args->out_data;
	const f64_t rh = args->rh,rw = args->rw;
	const uint32_t w = args->w,h = args->y1,ow = args->ow;
	register uint8_t* out_data;
	register const uint8_t *in_data;
	register f64_t frcx;
	f64_t frcy = (f64_t)args->y0 * rh;

	for (uint32_t y = args->y0,y1 = h;y < y1;++y,base += w,frcy += rh) {
		out_data = &out[base];
		in_data = &in[((uint32_t)frcy) * ow];
		frcx = 0.0;

		for (register uint32_t x = 0U;x < w;++x,frcx += rw) {
			*(out_data++) = in_data[(uint32_t)frcx];
		}
	}

	mt_entry_point_epilogue(NULL);
}

/*
	The parallel version of the bitmap importer & convertor
*/
static mt_entry_point_result_t bgr_to_gu8_entry_point( mt_entry_point_stack_args ) {
	mt_bgr_to_gu8_args_t* args;

	mt_entry_point_prologue(args,mt_bgr_to_gu8_args_t); 
	{
		const uint32_t bpp = args->bpp,padding = args->padding;
		const uint32_t y0 = args->y0,y1 = args->y1,width = args->width,height = args->height;
		uint8_t* in_wr_data = args->wr_data;
		register const uint8_t* rd_data = (const uint8_t*)&args->rd_data[y0 * ((width + padding) * bpp) ];
		register uint8_t* wr_data;
		
		uint32_t base = ((height-1U)-y0) * width;
		
		for (uint32_t i=y0; i < y1;++i,base-= width) {
			wr_data = &in_wr_data[base];
	 
			for (register uint32_t j=0U,k = width; j < k;++j,rd_data += bpp) {
				*(wr_data++) =  (uint8_t)( (f64_t)( g_pixel_chunk_b_lut[rd_data[0U]] + g_pixel_chunk_g_lut[rd_data[1U]] + g_pixel_chunk_r_lut[rd_data[2U]]) );
			}
			rd_data += padding;
		}
	}
	mt_entry_point_epilogue(NULL);
}

/*
	Compress a pixel chunk from GU8 -> GU1 
*/
pixel_chunk_t* pixel_chunk_compress(pixel_chunk_t* in_chunk) {
	pixel_chunk_t* out_chunk;

	if (!in_chunk) {
		return 0;
	} else if (in_chunk->compressed != 0U) {
		return pixel_chunk_duplicate(in_chunk);
	}
	
	out_chunk = new pixel_chunk_t;
	if (!out_chunk) {
		return 0;
	}

	const uint32_t r_size = align_offset( (in_chunk->w * in_chunk->h) >> 3U , 8U);
	out_chunk->angle = in_chunk->angle;
	out_chunk->w = in_chunk->w;
	out_chunk->h = in_chunk->h;
	out_chunk->bpp = 1U;
	out_chunk->stride = out_chunk->w >> 3U;
	out_chunk->compressed = 1U;
	out_chunk->pixels = (uint8_t*)heap_new(r_size);

	if (!out_chunk->pixels) {
		delete out_chunk;
		return 0;
	}

	uint32_t base = 0U;
	register uint8_t* wr_data = out_chunk->pixels;
	register const uint8_t* rd_data;

	//FIXME
	memset(out_chunk->pixels,0U,r_size); 

	for (uint32_t y0 = 0U,y1 = in_chunk->h;y0 < y1;++y0,base += out_chunk->w) {
		rd_data = &in_chunk->pixels[base];

		for (register uint32_t x0 = 0U,x1 = in_chunk->w;x0 < x1;++x0) {
			const uint32_t addr = (base + x0);
			wr_data[addr >> 3U] |= g_gu8_to_mono_lut[*(rd_data++)] << (addr & 7U);
		}
	}
	return out_chunk;
}

/*
	Duplicate a compatible pixel chunk 
*/
pixel_chunk_t* pixel_chunk_duplicate(pixel_chunk_t* in_chunk) {
	pixel_chunk_t* out_chunk;

	if (!in_chunk) {
		return 0;
	} 
	
	out_chunk = new pixel_chunk_t;
	if (!out_chunk) {
		return 0;
	}

	const uint32_t r_size = (in_chunk->compressed) ? align_offset( (in_chunk->w * in_chunk->h) >> 3U , 8U) : in_chunk->stride*in_chunk->h;
	out_chunk->angle = in_chunk->angle;
	out_chunk->w = in_chunk->w;
	out_chunk->h = in_chunk->h;
	out_chunk->bpp = in_chunk->bpp;
	out_chunk->stride = (in_chunk->compressed) ? align_offset(in_chunk->w >> 3U , 8U) : in_chunk->stride; 
	out_chunk->compressed = in_chunk->compressed;
	out_chunk->pixels = (uint8_t*)heap_new(r_size);

	if (!out_chunk->pixels) {
		delete out_chunk;
		return 0;
	}
 
	memcpy(out_chunk->pixels,in_chunk->pixels,r_size);

	return out_chunk;
}
 
/*
	Rotate a compatible pixel chunk (w/ interpolation)
*/

void pixel_chunk_rotate(pixel_chunk_t* in_chunk,const uint32_t angle,std::vector<uint32_t>& res) {
  
	const f64_t rad = deg_to_rad((f64_t)angle);
	const f64_t cosine = std::cos(rad);
	const f64_t sine = std::sin(rad);
 
	int32_t x1 = (int32_t)(-(int32_t)in_chunk->h * sine);
	int32_t y1 = (int32_t)((int32_t)in_chunk->h * cosine);
	int32_t x2 = (int32_t)((int32_t)in_chunk->w * cosine - (int32_t)in_chunk->h * sine);
	int32_t y2 = (int32_t)((int32_t)in_chunk->h * cosine + (int32_t)in_chunk->w * sine);
	int32_t x3 = (int32_t)((int32_t)in_chunk->w * cosine);
	int32_t y3 = (int32_t)((int32_t)in_chunk->w * sine);
	int32_t minx = (int32_t)std::min(0,(int32_t)std::min(x1,(int32_t)std::min(x2,x3)));
	int32_t miny = (int32_t)std::min(0,(int32_t)std::min(y1,(int32_t)std::min(y2,y3)));
	int32_t maxx = (int32_t)std::max(x1,(int32_t)std::max(x2,x3));
	int32_t maxy = (int32_t)std::max(y1,(int32_t)std::max(y2,y3));

	int32_t w = (int32_t)std::ceil(std::fabs(maxx)-minx); 
	int32_t h = (int32_t)std::ceil(std::fabs(maxy)-miny); 

	res.clear();

	for (int32_t y = 0,y_base = 0; y < h; ++y,y_base += w) {
		for (int32_t x = 0; x < w; ++x) {
	 		int64_t u = (int64_t)(f64_t) ( (f64_t)(minx+x) * std::cos(rad) + (f64_t)(miny+y) * std::sin(rad) );
			int64_t v = (int64_t)(f64_t) ( (f64_t)(miny+y) * std::cos(rad) - (f64_t)(minx+x) * std::sin(rad) ) ;

			if( (u >= 0) && (u < (int32_t)in_chunk->w) && (v >= 0) && (v < (int32_t)in_chunk->h)) {
				//out_chunk->pixels[y_base + x] = in_chunk->pixels[v * in_chunk->stride + u];
			 	res.push_back(x);
			 	res.push_back(y);
			 	res.push_back(in_chunk->pixels[v * in_chunk->stride + u]);
			}
			 
		}
	}
 
}

 

/*
	Upscale a pixel chunk 
*/
pixel_chunk_t* pixel_chunk_upscale(pixel_chunk_t* in_chunk,const uint32_t factor) {
	pixel_chunk_t* out_chunk;

	if (in_chunk == 0) {
		return 0;
	}

	out_chunk = new pixel_chunk_t;
	if (out_chunk == 0) {
		return 0;
	}

	out_chunk->angle = in_chunk->angle;
	out_chunk->w = in_chunk->w * factor;
	out_chunk->h = in_chunk->h * factor;
	out_chunk->bpp = in_chunk->bpp;
	out_chunk->stride = out_chunk->w * out_chunk->bpp;
	out_chunk->compressed = 0U;
	out_chunk->pixels = (uint8_t*)heap_new(out_chunk->h * out_chunk->stride);
 
	if (!out_chunk->pixels) {
		delete out_chunk;
		return 0;
	}
 
	const f64_t wr = (f64_t)out_chunk->w / (f64_t)in_chunk->w;
	const f64_t hr = (f64_t)out_chunk->h / (f64_t)in_chunk->h;
	const f64_t rw = 1.0 / wr;
	const f64_t rh = 1.0 / hr;
	uint32_t base = 0;
	register uint8_t* out_data;
	register const uint8_t *in_data;
	register f64_t frcx;
	f64_t frcy = 0.0;

	for (uint32_t y = 0U,y1 = out_chunk->h;y < y1;++y,base += out_chunk->w,frcy += rh) {

		/*
			(u32)frcy increases every 1.0/(1.0 / hr) steps
		*/
		out_data = &out_chunk->pixels[base];
		in_data = &in_chunk->pixels[((uint32_t)frcy) * in_chunk->w];
		frcx = 0.0;

		/*
			(u32)frcx increases every 1.0/(1.0 / wr) steps
		*/
		for (register uint32_t x = 0U,x1 = out_chunk->w;x < x1;++x,frcx += rw) {
			*(out_data++) = in_data[(uint32_t)frcx];
		}
	}
 
	return out_chunk;
}

/*
	Upscale a pixel chunk using simple NN scaling (MT)
*/

pixel_chunk_t* pixel_chunk_upscale_mt(pixel_chunk_t* in_chunk,const uint32_t factor) {
	pixel_chunk_t* out_chunk;

	if (in_chunk == 0) {
		return 0;
	}

	out_chunk = new pixel_chunk_t;
	if (out_chunk == 0) {
		return 0;
	}

	out_chunk->angle = in_chunk->angle;
	out_chunk->w = in_chunk->w * factor;
	out_chunk->h = in_chunk->h * factor;
	out_chunk->bpp = in_chunk->bpp;
	out_chunk->stride = out_chunk->w * out_chunk->bpp;
	out_chunk->pixels = (uint8_t*)heap_new(out_chunk->h * out_chunk->stride);
 
	if (!out_chunk->pixels) {
		delete out_chunk;
		return 0;
	}
 
	const f64_t wr = (f64_t)out_chunk->w / (f64_t)in_chunk->w;
	const f64_t hr = (f64_t)out_chunk->h / (f64_t)in_chunk->h;
	const f64_t rw = 1.0 / wr;
	const f64_t rh = 1.0 / hr;

	std::vector<mt_upscaler_args_t> args;
	std::vector<mt_extent_t> extent;

	const uint32_t max_th = mt_get_thread_contexts();

	mt_calculate_extent(extent,out_chunk->h,max_th);

	const uint32_t active_th = extent.size();
	args.reserve(active_th);
 
	for (uint32_t i = 0U;i < active_th;++i) {
		mt_upscaler_args_t arg;
 
		arg.in_data = in_chunk->pixels;
		arg.out_data = out_chunk->pixels;
		arg.w = out_chunk->w;
		arg.h = out_chunk->h;
		arg.ow = in_chunk->w;
		arg.y0 = extent[i].s0;
		arg.y1 = extent[i].s1;
		arg.rw = rw;
		arg.rh = rh;
		args.push_back(arg);
		mt_call_thread(upscaler_entry_point,(void*)&args[i],i);
	}
	mt_wait_threads(active_th);

	return out_chunk;
}

/*
	Create an 8bit pixel chunk from a given RGB/RGBA stream
	XXX : Not thread safe since it will use mt() module if file size >= 1*MB without checking for active tasks
*/

pixel_chunk_t* pixel_chunk_new(const std::string& path) {
	pixel_chunk_t* chunk;
	bmp_header_t* header;
	uint8_t* heap_entry;
	uint8_t* vfile;
    int32_t fd;
    uint64_t fsize;
    struct stat64 st;

	/*Open stream*/
    fd = open64(path.c_str(),O_RDONLY);
	if (fd < 0) {
		printf("Can't open64(+R) %s\n",path.c_str());
		return 0;
	}

    if (fstat64(fd,&st) < 0) {
		printf("Can't fstat64 %s\n",path.c_str());
		close(fd);
		return 0;
	}

    fsize = st.st_size;
 
	chunk = new pixel_chunk_t;
	if (!chunk) {	
		printf("pixel_chunk_new : chunk==0\n");
		close(fd);
		return 0;
	}

	/*Read whole file to memory to prevent extra reads/seeks*/

	heap_entry = (uint8_t*)heap_new(fsize);
	if (!heap_entry) {
		printf("Out of memory allocating %lu bytes\n",(uint64_t)fsize);
		close(fd);
		delete chunk;
		return 0;
	}

	vfile = heap_entry ;
    read(fd,(void*)vfile,fsize);

	header = (bmp_header_t*)&vfile[0U];
	chunk->w = header->width;
	chunk->h = header->height;
	chunk->compressed = 0U;
	chunk->angle = 0U;
		 
	const uint32_t bytes_per_pixel = header->bit_per_pixel >> 3U;
	const uint32_t padding = ((header->width*bytes_per_pixel) % 4U == 0U) ? 0U : 4U - ((header->width*bytes_per_pixel) % 4U);
		
	chunk->bpp = 1U;
	chunk->stride = chunk->w * chunk->bpp;
	chunk->pixels = (uint8_t*)heap_new(chunk->h * chunk->stride);
	if (!chunk->pixels) {
		close(fd);
		heap_delete(heap_entry);
		pixel_chunk_delete(chunk);
		return 0;
	}
 
	if (header->height == 0U) {
		printf("(u32) header->height == 0 == underflow ...\n");
	}

	/*Switch between single/multi thread importer+convertor at runtime*/
	if ( (fsize < (1*1024*1024)) || (mt_get_thread_contexts() < 2U) ) {
		register const uint8_t* rd_data = &vfile[header->offset];
		register uint8_t* wr_data = chunk->pixels;
		uint32_t base = (header->height-1U) * header->width;
 
		for (uint32_t i=0,l = header->height; i < l;++i,base -= header->width) {
			wr_data = &chunk->pixels[base];
			for (register uint32_t j=0U,k = header->width; j < k;++j,rd_data += bytes_per_pixel) {
 
				*(wr_data++) = (uint8_t)( (f64_t)( g_pixel_chunk_b_lut[rd_data[0U]] + g_pixel_chunk_g_lut[rd_data[1U]] + g_pixel_chunk_r_lut[rd_data[2U]]) );
			}
			rd_data+=padding;			
		}
	} else {
		/*MT Import+convert : Subdivide by height*/
		std::vector<mt_bgr_to_gu8_args_t> args;
		std::vector<mt_extent_t> extent;
		const uint32_t max_th = mt_get_thread_contexts();

		mt_calculate_extent(extent,header->height,max_th);

		const uint32_t active_th = extent.size();
		args.reserve(active_th);

		for (uint32_t i = 0;i < active_th;++i) {
			mt_bgr_to_gu8_args_t arg;
				
			arg.y0 = extent[i].s0;
			arg.y1 = extent[i].s1;
			arg.bpp = bytes_per_pixel;
			arg.padding = padding;
			arg.width = header->width;
			arg.height = header->height;
			arg.rd_data = &vfile[header->offset];
			arg.wr_data = chunk->pixels;
			args.push_back(arg);
			mt_call_thread(bgr_to_gu8_entry_point,(void*)&args[i],i);
		}
		mt_wait_threads(active_th);
	}
		
	heap_delete(heap_entry);
	close(fd);
	return chunk;
}

pixel_chunk_t* pixel_chunk_new_mapped(const std::string& path) {
#ifndef __USE_MAPPED_IO__
	return pixel_chunk_new(path);
#else
	
	const size_t max_rd_len = 1U * 1024U * 1024U * 1024U; 
	pixel_chunk_t* chunk;
	bmp_header_t header;
    int32_t fd;
    uint64_t fsize,pix_len,pix_base;
    struct stat64 st;

	/*Open stream*/
    fd = open(path.c_str(),O_RDONLY);
	if (fd < 0) {
		return 0;
	}

    if (fstat64(fd,&st) < 0) { 
		return 0; 
	}

    fsize = st.st_size;

	chunk = new pixel_chunk_t;
	if (!chunk) {
		close(fd);
		return 0;
	}

	/*Copy hdr*/
	{
		uint8_t* hdr = (uint8_t*)mmap(0,sizeof(bmp_header_t),PROT_READ,MAP_FILE|MAP_PRIVATE,fd,0);
		if (hdr == MAP_FAILED) {
			printf("mmap failed - Will fallback to standard impl.\nTo disable mapped IO operations #undef __USE_MAPPED_IO__ in include/types.hpp \n");
			close(fd);
			delete chunk;
			return pixel_chunk_new(path); /*FALLBACK!*/
		}

		memcpy(&header,hdr,sizeof(bmp_header_t));
		munmap((void*)hdr,sizeof(bmp_header_t));
	}
	
	//printf("%u %u %u\n",header.offset,header.width,header.height); while(1);
	const uint32_t bytes_per_pixel = header.bit_per_pixel >> 3U;
	const uint32_t padding = ((header.width*bytes_per_pixel) % 4U == 0U) ? 0U : 4U - ((header.width*bytes_per_pixel) % 4U);

	/*Fill chunk info*/
	pix_base = header.offset;
	pix_len = fsize - pix_base;

	/*Just the 1/8 of the actual file size has to be allocated*/
	chunk->w = header.width;
	chunk->h = header.height;
	chunk->compressed = 0U;
	chunk->angle = 0U;		
	chunk->bpp = 1U;
	chunk->stride = chunk->w * chunk->bpp;
	chunk->pixels = (uint8_t*)heap_new(chunk->h * chunk->stride);

	if (!chunk->pixels) {
		pixel_chunk_delete(chunk);
		close(fd);
		printf("Pixel chunk : OUT OF MEMORY allocating %u bytes \n",chunk->h * chunk->stride);
		return 0;
	}
;

	if (header.height == 0U) {
		printf("(u32) header.height == 0 == underflow ...\n");
	}

	uint64_t n_slices = (((bytes_per_pixel * header.width) + padding) * header.height);
	uint64_t n_hdr_height_slice;

	if (n_slices <= (uint64_t)max_rd_len) {
		n_slices = 1U;
		n_hdr_height_slice = header.height;
	} else {
		n_slices /= max_rd_len;
		n_hdr_height_slice = header.height / n_slices;
	}
	const size_t page_size = sysconf(_SC_PAGE_SIZE);
	const uint64_t blk_len = (((bytes_per_pixel * header.width) + padding) * n_hdr_height_slice);
	uint64_t blk_len_a = (blk_len + page_size) & (~(page_size-1));

	if (blk_len_a > (uint64_t)max_rd_len) { blk_len_a = (uint64_t)max_rd_len; }

	/*MT Import+convert : Subdivide by height*/
	uint64_t base_y = 0U;
	uint32_t max_th = mt_get_thread_contexts();

	if (max_th < 1U) { 
		max_th = 1U;
	}

	for (uint64_t i = 0U;i < n_slices;++i,base_y += n_hdr_height_slice,pix_base += blk_len) {
 		const off_t patch_diff = pix_base - ((off_t)(pix_base ) & (~(page_size - 1)));


		if ((pix_base + blk_len_a ) > pix_len) { 
			blk_len_a = pix_len-pix_base;
		}

		const off_t patch_bp_exp_len = ((blk_len_a + patch_diff) <= pix_len) ? blk_len_a + patch_diff : blk_len_a;
		uint8_t* pix = (uint8_t*)mmap(0,(size_t)blk_len_a + patch_bp_exp_len,PROT_READ,MAP_FILE|MAP_PRIVATE,fd,
		(off_t)(pix_base) & (~(page_size - 1)) ) ;
		if (pix == MAP_FAILED) {
			printf("mmap failed - Will fallback to standard impl.\nTo disable mapped IO operations #undef __USE_MAPPED_IO__ in include/types.hpp \n");
			close(fd);
			pixel_chunk_delete(chunk);
			return pixel_chunk_new(path);
		}
		std::vector<mt_bgr_to_gu8_args_t> args;
		std::vector<mt_extent_t> extent;
		

		mt_calculate_extent(extent,n_hdr_height_slice,max_th);

		const uint32_t active_th = extent.size();
		args.reserve(active_th);

		for (uint32_t i = 0;i < active_th;++i) {
			mt_bgr_to_gu8_args_t arg;
				
			arg.y0 = (uint32_t)base_y + extent[i].s0;
			arg.y1 = (uint32_t)base_y + extent[i].s1;
			arg.bpp = bytes_per_pixel;
			arg.padding = padding;
			arg.width = header.width;
			arg.height = header.height;
			arg.rd_data = &pix[patch_diff];
			arg.wr_data = chunk->pixels;
			args.push_back(arg);

			mt_call_thread(bgr_to_gu8_entry_point,(void*)&args[i],i);
		}
		mt_wait_threads(active_th);

		munmap((void*)pix,(size_t)blk_len_a);
	}

	close(fd);
	return chunk;
#endif
}

void pixel_chunk_delete(pixel_chunk_t* chunk) {
	if (chunk == 0) {
		return;
	}
	
	heap_delete(chunk->pixels);
	delete chunk;
}

/*
	Write a pixel chunk to a given file path (DEBUG FUNCTION-NON OPTIMIZED)
*/
bool pixel_chunk_dump(pixel_chunk_t* in_chunk,const std::string& path) {
	if (!in_chunk) {
		return false;
	} else if (in_chunk->compressed != 0U) {
		return false;
	}

	FILE* f = fopen(path.c_str(),"wb");
	if (!f) {
		return false;
	}

	const uint32_t w = in_chunk->w,h = in_chunk->h ;
	uint8_t header[18U];

	memset(header,0U,sizeof(header));
	header[2U] = 2U; 
	header[12U] = w & 0xffU;
	header[13U] = (w >> 8U) & 0xffU;
	header[14U] = h & 0xffU;
	header[15U] = (h >> 8U) & 0xffU;
	header[16U] = 24U; 
	fwrite(header,1U,sizeof(header),f);

	for (uint32_t y = 0U;y < h;++y) {
		for (uint32_t x = 0U;x < w;++x) {
			uint8_t pv = in_chunk->pixels[(h-y-1U) * w + x];
			//pv = (pv < 128) ? 0 : 0xff;
			for (uint32_t z = 0U;z < 3U;++z) {//repeat gray pixel for all components
				fputc(pv,f);
			}
		}
	}

	fclose(f);
	return true;
}


