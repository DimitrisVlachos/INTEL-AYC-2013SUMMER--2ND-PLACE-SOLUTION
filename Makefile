CC=$(ICPC_SDK_PATH)icpc
OPTIMIZATION=-fast -fno-exceptions -fno-rtti -fno-alias -fno-fnalias -fargument-noalias 
CFLAGS=-ansi -std=c++11 -pedantic -Iincludes -pthread -D_LARGE_FILE_API \
-D__LIB_AH_DEFAULT_ALIGNMENT__=64 -D__LIB_AH_HDR_SIZE__=256
CFLAGS_DEBUG=-W -g -Wall -ansi -std=c++11 -pthread -pedantic -Iincludes -O0 -check-uninit -debug all -D_LARGE_FILE_API \
-D__LIB_AH_DEFAULT_ALIGNMENT__=64 -D__LIB_AH_HDR_SIZE__=256
CFLAGS_MIC=-ansi -std=c++11  -pedantic -Iincludes -D__MIC__BUILD__ -pthread -mmic
LDFLAGS=-pthread $(OPTIMIZATION)
LDFLAGS_DEBUG=-pthread -O0 -check-uninit -debug all -D_LARGE_FILE_API -D__LIB_AH_DEFAULT_ALIGNMENT__=64 -D__LIB_AH_HDR_SIZE__=256
LDFLAGS_MIC=-pthread $(OPTIMIZATION) -D__MIC__BUILD__ -D__LIB_AH_DEFAULT_ALIGNMENT__=64 -D__LIB_AH_HDR_SIZE__=256 -mmic
EXEC=run
SRC=$(wildcard src/*.cpp)
OBJ=$(SRC:src/%.cpp=obj/%.o)
OBJ_MIC=$(SRC:src/%.cpp=obj/%.omic)
OBJ_DEBUG=$(SRC:src/%.cpp=obj/%.odeb)

#TEAM_ID = e3b3f64b2db735915b5ee8d113f81073


all:$(EXEC)

mic:$(OBJ_MIC)
	$(CC) -o $@ $^ $(LDFLAGS_MIC)

default:all

debug:$(OBJ_DEBUG)
	$(CC) -o $@ $^ $(LDFLAGS_DEBUG)


run:$(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

obj/%.o:src/%.cpp
	$(CC) -o $@ -c $< $(CFLAGS)

obj/%.omic:src/%.cpp
	$(CC) -o $@ -c $< $(CFLAGS_MIC)

obj/%.odeb:src/%.cpp
	$(CC) -o $@ -c $< $(CFLAGS_DEBUG)

clean:
	rm -f obj/*.o* $(EXEC) *.zip debug

zip: clean
ifdef TEAM_ID
	zip $(strip $(TEAM_ID)).zip -9r Makefile src/ obj/ includes/
else
	@echo "you need to put your TEAM_ID in the Makefile"
endif

#submit: zip
#ifdef TEAM_ID
#	curl -F "file=@$(strip $(TEAM_ID)).zip" -L http://www.intel-software-academic-program.com/contests/ayc/upload/upload.php
#else
	@echo "you need to put your TEAM_ID in the Makefile"
#endif


