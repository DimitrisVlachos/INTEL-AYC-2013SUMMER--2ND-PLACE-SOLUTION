
/*
	SSE# bit mask jump LUT generator
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com)
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
 
void emit_text(FILE* f,const char* t) {
	fwrite((const void*)t,1,strlen(t),f);
}

void generate_eq(FILE* f,const char* field) {
	uint32_t i;
 	uint32_t j;

	emit_text(f,"static const uint8_t ");
	emit_text(f,field);
	emit_text(f,"[] __attribute__ ((aligned (64)))  = {\n\t");

	for (i = 0U;i <= 0xffff;++i) {

		for (j = 0U;j < 16U;++j) {
			uint32_t tst = !!(i & (1U << j));
			if (tst == 0U) {
				break;
			}
		}
		fprintf(f,"%dU,",j);
#if 0
		emit_text(f,"\t\t//");
		for (j = 0U;j < 16U;++j) {
			uint32_t tst = !!(i & (1 << j ));
			fprintf(f,"%d",tst);
		}
		emit_text(f,"\n\t");
#else
		if (i && (!(i & 15))) {
			emit_text(f,"\n\t");
		}
#endif
	}

	emit_text(f,"\n\t};\n");	
}

void generate_neq(FILE* f,const char* field) {
	uint32_t i;
 	uint32_t j;

	emit_text(f,"static const uint8_t ");
	emit_text(f,field);
	emit_text(f,"[] __attribute__ ((aligned (64)))  = {\n\t");

	for (i = 0U;i <= 0xffff;++i) {

		for (j = 0U;j < 16U;++j) {
			uint32_t tst = !!(i & (1U << j));
			if (tst == 1U) {
				break;
			}
		}
		fprintf(f,"%dU,",j);
#if 0
		emit_text(f,"\t\t//");
		for (j = 0U;j < 16U;++j) {
			uint32_t tst = !!(i & (1 << j ));
			fprintf(f,"%d",tst);
		}
		emit_text(f,"\n\t");
#else
		if (i && (!(i & 15))) {
			emit_text(f,"\n\t");
		}
#endif
	}

	emit_text(f,"\n\t};\n");	
}

int main() {
	FILE* f = fopen("sse_msk_luts.h","wb");

	emit_text(f,"#ifndef __sse_msk_jmp_lut__\n#define __sse_msk_jmp_lut__\n\n\t");
	generate_eq(f,"g_sse_msk_jmp_eq_lut");
	generate_neq(f,"g_sse_msk_jmp_neq_lut");
	emit_text(f,"#endif\n\n");
	fclose(f);
	return 0;
}

