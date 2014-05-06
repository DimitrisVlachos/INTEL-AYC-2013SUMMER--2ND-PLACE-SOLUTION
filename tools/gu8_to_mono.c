
/*
	Grayscale(8bit) -> Mono(1bit) LUT generator
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com)
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

void emit_text(FILE* f,const char* t) {
	fwrite((const void*)t,1,strlen(t),f);
}

void generate(FILE* f,const char* field) {
	uint32_t i;
 
	emit_text(f,"static const uint8_t ");
	emit_text(f,field);
	emit_text(f,"[] __attribute__ ((aligned (64)))  = {\n\t");
	
	/*b0 = Black , b1 = White*/
	for (i = 0U;i < 256U;++i) {
		fprintf(f,"%uU,",i >= 128U);
		if (i && (!(i & 7))) {
			emit_text(f,"\n\t");
		}
	}

	emit_text(f,"\n\t};\n");

	
}

int main() {
	FILE* f = fopen("gu8_to_mono.h","wb");

	emit_text(f,"#ifndef __gu8_2_mono__\n#define __gu8_2_mono__\n\n\t");
	generate(f,"g_gu8_to_mono_lut");
	emit_text(f,"#endif\n\n");
	fclose(f);
	return 0;
}

