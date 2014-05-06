/*
	Builtin & custom types
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#ifndef __types__hpp__
#define __types__hpp__

#undef __USE_MAPPED_IO__
#include <stdint.h>
#include <stddef.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <list>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>


#ifndef __MIC__BUILD__
	#include <emmintrin.h>
	#include <xmmintrin.h>
#else
	#include <immintrin.h>
	#include <zmmintrin.h>
#endif


typedef float f32_t;
typedef double f64_t;
typedef uint32_t boolean_t;

struct parameters_t {
	uint32_t nb_threads,max_scale,max_rot;
	std::string main_image_name;
	std::list<std::string> template_names;
};

struct result_t {
	uint32_t pattern;
	uint32_t px;
	uint32_t py;
	uint32_t _align;
};

typedef struct __attribute__ ((__packed__)){
	//BMP header
	uint16_t magic_number;
	uint32_t size;
	uint32_t reserved;
	uint32_t offset;
	//DIB header
	uint32_t dibSize;
	uint32_t width;
	uint32_t height;
	uint16_t plane;
	uint16_t bit_per_pixel;
	uint32_t compression;
	uint32_t data_size;
	uint32_t hor_res;
	uint32_t vert_res;
	uint32_t color_number;
	uint32_t important;
}bmp_header_t;


#endif

