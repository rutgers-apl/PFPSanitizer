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
 headers.h\
 HYPRE_parcsr_mv.h\
 _hypre_parcsr_mv.h\
 new_commpkg.h\
 par_csr_communication.h\
 par_csr_matrix.h\
 par_vector.h

all: parcsr_mv.fp.bc libparcsr_mv.a

libparcsr_mv_fp.a: seq_opt_fp.o ${HEADERS} 
	@echo  "Building $@ ..."
	ar -rcu $@ $^
	${RANLIB} $@

seq_opt_fp.o: seq_opt_fp.bc 
	llc $< -filetype=obj -o $@

seq_opt_fp.bc: parcsr_mv.fp.o
	$(OPT) <seq_fp.bc> $@ 

parcsr_mv.fp.bc: $(OBJ_FILES_FP)
	llvm-link $^ -o $@ 

$(OBJ_DIR)/%.bc: $(SRC_DIR)/%.c
	$(CCE) $(CXXFLAGS) -c -o $@ $< 

libparcsr_mv.a: $(OBJ_FILES)
	@echo  "Building $@ ..." 
	ar -rcu $@ $^
	${RANLIB} $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf  libparcsr_mv.a libparcsr_mv_fp.a *.out *.o *.dSYM *.bc seq_opt_fp
