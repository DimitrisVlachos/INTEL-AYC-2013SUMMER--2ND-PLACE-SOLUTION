
/*
	Sin/Cos LUT generator
	Author :  Dimitris Vlachos (DimitrisV22@gmail.com)
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

static inline const double deg_to_rad(const double angle) {
	return (double)angle * 3.14159265359 / 180.0;
}

void emit_text(FILE* f,const char* t) {
	fwrite((const void*)t,1,strlen(t),f);
}

void generate_sin_lut(FILE* f,const char* field) {
	uint32_t i;
 
	emit_text(f,"static const double ");
	emit_text(f,field);
	emit_text(f,"[] __attribute__ ((aligned (64)))  = {\n\t");

	for (i = 0U;i < 360U;++i) {
		const double rad = deg_to_rad((double)i);
		const double sine = (double)sin(rad);
		fprintf(f,"%lf,",sine);
		if (i && (!(i & 7))) {
			emit_text(f,"\n\t");
		}
	}

	emit_text(f,"\n\t};\n");	
}

void generate_cos_lut(FILE* f,const char* field) {
	uint32_t i;
 
	emit_text(f,"const double ");
	emit_text(f,field);
	emit_text(f,"[] __attribute__ ((aligned (64)))  = {\n\t");

	for (i = 0U;i < 360U;++i) {
		const double rad = deg_to_rad((double)i);
		const double cosine = (double)cos(rad);
		fprintf(f,"%lf,",cosine);
		if (i && (!(i & 7))) {
			emit_text(f,"\n\t");
		}
	}

	emit_text(f,"\n\t};\n");	
}

int main() {
	FILE* f = fopen("sincos.h","wb");

	emit_text(f,"#ifndef __sin_cos__\n#define __sin_cos__\n\n\t");
	generate_sin_lut(f,"g_sin_lut");
	generate_cos_lut(f,"g_cos_lut");
	emit_text(f,"#endif\n\n");
	fclose(f);
	return 0;
}

