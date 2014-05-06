/*
	lib_ah : Aligned heap wrapper/library
	Author : Dimitris Vlachos (DimitrisV22@gmail.com @ github.com/DimitrisVlachos)
*/

#ifndef __aligned_heap__h__
#define __aligned_heap__h__

extern "C" {
	#include <stdint.h>
	#include <malloc.h>

	/*
		pass -D__LIB_AH_DEFAULT_ALIGNMENT__=alignment for custom alignment
	*/
	#ifdef __LIB_AH_DEFAULT_ALIGNMENT__
		#define k_heap_alignment (__LIB_AH_DEFAULT_ALIGNMENT__)
	#else
		#define k_heap_alignment (64U)
	#endif

	#define k_heap_alignment_mask (k_heap_alignment - 1U)

	#define heap_align_attr __attribute__ ((aligned (k_heap_alignment)))
	#define heap_align_attr_imm(_imm_) __attribute__ ((aligned (_imm_)))

	#ifndef __restrict__
		#define __restrict__ 
	#endif

	typedef double* __restrict__ heap_align_attr_imm(sizeof(double)) double_ptr_t;
	typedef float* __restrict__ heap_align_attr_imm(sizeof(float))  float_ptr_t;
	typedef void* __restrict__ heap_align_attr ptr_t;
	typedef uint8_t* __restrict__ uint8_ptr_t;
	typedef uint16_t* __restrict__ heap_align_attr_imm(sizeof(uint16_t)) uint16_ptr_t;
	typedef uint32_t* __restrict__ heap_align_attr_imm(sizeof(uint32_t)) uint32_ptr_t;
	typedef uint64_t* __restrict__ heap_align_attr_imm(sizeof(uint64_t)) uint64_ptr_t;
	typedef int8_t* __restrict__ int8_ptr_t;
	typedef int16_t* __restrict__ heap_align_attr_imm(sizeof(int16_t)) int16_ptr_t;
	typedef int32_t* __restrict__ heap_align_attr_imm(sizeof(int32_t)) int32_ptr_t;
	typedef int64_t* __restrict__ heap_align_attr_imm(sizeof(int64_t)) int64_ptr_t;

	void* heap_new(uint64_t size);
	void heap_delete(void* ent);
}
#endif


