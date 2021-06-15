CC = clang -pg -O1
CCE = clang -pg -O1 -fno-inline -emit-llvm 
OPT = opt -load $(PFSAN_HOME)/llvm-pass/build/FPSan/libFPSanitizer.so -fpsan
GCC := clang -pg -O1
SRC_DIR := .
OBJ_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
OBJ_FILES_FP := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.bc,$(SRC_FILES))
CPPFLAGS := -g -std=c11  -L$(MPFR_HOME)/src/.libs/ -lmpfr -lm  -lpthread ../polybench.c
CFLAGS= -fno-slp-vectorize -std=c11 -L$(PFSAN_HOME)/runtime/obj -lfpsanitizer -L$(MPFR_HOME)/src/.libs/ -lmpfr -lm  -lstdc++ -L$(TBB_HOME)/build -ltbb -I$(MPFR_HOME)/include
CXXFLAGS := -g -I../  -fno-strict-aliasing  

all: cholesky_fp.bc cholesky_opt_fp.bc cholesky_opt_fp.o  cholesky cholesky_fp
#all: cholesky_fp.bc  cholesky 

cholesky_fp:cholesky_opt_fp.o 
	$(CC) $^ -o $@  $(CPPFLAGS) $(CFLAGS)

cholesky_opt_fp.o: 
	llc cholesky_opt_fp.bc -filetype=obj  -o $@ $^

cholesky_opt_fp.bc: 
	$(OPT) <cholesky_fp.bc> $@ $^

cholesky_fp.bc: $(OBJ_FILES_FP)
	llvm-link $^ -o $@ 

$(OBJ_DIR)/%.bc: $(SRC_DIR)/%.c
	$(CCE) $(CXXFLAGS) -c -o $@ $< 

cholesky: $(OBJ_FILES)
	$(GCC) $(CPPFLAGS) -o $@ $^ 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf  cholesky cholesky_fp cholesky.o  *.out *.o *.dSYM *.bc *.ll cholesky_opt_fp
