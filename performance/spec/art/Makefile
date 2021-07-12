CC = clang -O1
CCE = clang -O1 -fno-inline -emit-llvm 
OPT = opt -load  $(FPSAN_HOME)/llvm-pass/build/FPSan/libFPSanitizer.so -fpsan
GCC := clang -O1
SRC_DIR := .
OBJ_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
OBJ_FILES_FP := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.bc,$(SRC_FILES))
CPPFLAGS := -g -std=c11  -L$(MPFR_HOME)/src/.libs/  -lmpfr -lm  -lpthread
CFLAGS= -fno-slp-vectorize -std=c11 -L$(FPSAN_HOME)/runtime/obj -lfpsanitizer -L$(MPFR_HOME)/src/.libs/  -lmpfr -lm  -lstdc++ -L$(TBB_HOME)/build -ltbb -I$(MPFR_HOME)/include
CXXFLAGS :=  -g -I . -I ./libutil -DSPEC_CPU -DNDEBUG -I. -DFN -DFAST -DCONGRAD_TMP_VECTORS -DDSLASH_TMP_LINKS   -fno-strict-aliasing -DSPEC_CPU_LP64  

all: art_fp.bc art_opt_fp.bc art_opt_fp.o art_fp art 

art_fp:art_opt_fp.o 
	$(CC) $^ -o $@  $(CPPFLAGS) $(CFLAGS)

art_opt_fp.o: 
	llc art_opt_fp.bc -filetype=obj  -o $@ $^

art_opt_fp.bc: 
	$(OPT) <art_fp.bc> $@ $^

art_fp.bc: $(OBJ_FILES_FP)
	llvm-link $^ -o $@ 

$(OBJ_DIR)/%.bc: $(SRC_DIR)/%.c
	$(CCE) $(CXXFLAGS) -c -o $@ $< 

art: $(OBJ_FILES)
	$(GCC) $(CPPFLAGS) -o $@ $^ 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf art art_fp art.o art *.out *.o *.dSYM *.bc *.ll art_opt_fp
