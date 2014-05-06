
/*
	BGR -> Grayscale 8bit LUT generator
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com)
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

void emit_text(FILE* f,const char* t) {
	fwrite((const void*)t,1,strlen(t),f);
}

void generate(FILE* f,const char* field,const double frac) {
	uint32_t i;
 
	emit_text(f,"static const f64_t ");
	emit_text(f,field);
	emit_text(f,"[] __attribute__ ((aligned (64)))  = {\n\t");
	
	for (i = 0U;i < 256U;++i) {
		fprintf(f,"%lf,", frac * (double)i);
		if (i && (!(i & 7))) {
			emit_text(f,"\n\t");
		}
	}

	emit_text(f,"\n\t};\n");	
}

int main() {
	FILE* f = fopen("bgr2gu8.h","wb");

	emit_text(f,"#ifndef __bgr2gu8__\n#define __bgr2gu8__\n\n\t");
	generate(f,"g_pixel_chunk_b_lut",0.0721);
	generate(f,"g_pixel_chunk_g_lut",0.7154);
	generate(f,"g_pixel_chunk_r_lut",0.2125);
	emit_text(f,"#endif\n\n");
	fclose(f);
	return 0;
}

