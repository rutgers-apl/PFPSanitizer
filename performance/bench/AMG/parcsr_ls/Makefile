CC = clang -pg
CCE = clang -pg -fno-inline -emit-llvm 
OPT = opt -load $(FPSAN_HOME)/llvm-pass/build/FPSan/libFPSanitizer.so -fpsan
GCC := clang -pg
SRC_DIR := .
OBJ_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
OBJ_FILES_FP := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.bc,$(SRC_FILES))
CPPFLAGS := -g -std=c11  -lmpfr -lm  -lpthread
CFLAGS= -fno-slp-vectorize -std=c11 -L$(FPSAN_HOME)/runtime/obj -lfpsanitizer -lmpfr -lm  -lstdc++ -L$(FPSAN_HOME)/tbb/build -ltbb -I$(MPFR_HOME)/include
INCLUDE_CFLAGS = -g -O1 -DHYPRE_SEQUENTIAL=1
CXXFLAGS :=  -g -I.. -I../utilities -I../krylov -I../seq_mv -I../parcsr_mv -I../IJ_mv ${INCLUDE_CFLAGS}
RANLIB = ranlib

HEADERS =\
 ams.h\
 HYPRE_parcsr_ls.h\
 _hypre_parcsr_ls.h\
 aux_interp.h\
 headers.h\
 par_amg.h

all: parcsr_ls.fp.bc libparcsr_ls.a

libparcsr_ls_fp.a: seq_opt_fp.o 
	@echo  "Building $@ ..."
	ar -rcu $@ $^
	${RANLIB} $@

seq_opt_fp.o: seq_opt_fp.bc 
	llc $< -filetype=obj -o $@

seq_opt_fp.bc: parcsr_ls.fp.bc
	$(OPT) <parcsr_ls.fp.bc> $@ 

parcsr_ls.fp.bc: $(OBJ_FILES_FP)
	llvm-link $^ -o $@ 

$(OBJ_DIR)/%.bc: $(SRC_DIR)/%.c
	$(CCE) $(CXXFLAGS) -c -o $@ $< 

libparcsr_ls.a: $(OBJ_FILES)
	@echo  "Building $@ ..." 
	ar -rcu $@ $^
	${RANLIB} $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf  libIJ_mv_fp.a libIJ_mv_.a  *.out *.o *.dSYM *.bc seq_opt_fp
