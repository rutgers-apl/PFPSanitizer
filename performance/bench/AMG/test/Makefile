CC = clang
CCE = clang -fno-inline -emit-llvm 
OPT = opt -load $(FPSAN_HOME)/llvm-pass/build/FPSan/libFPSanitizer.so -fpsan
GCC := clang
SRC_DIR := .
OBJ_DIR := .
SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
OBJ_FILES_FP := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.bc,$(SRC_FILES))
CPPFLAGS := -g -std=c11   -L$(MPFR_HOME)/src/.libs/ -lmpfr -lm  -lpthread
FPSANX_LFLAGS= -fno-slp-vectorize -std=c11 -L$(FPSAN_HOME)/runtime/obj -lfpsanitizer  -L$(MPFR_HOME)/src/.libs/ -lmpfr -lm  -lstdc++ -L$(TBB_HOME)/build -ltbb -I$(MPFR_HOME)/include
INCLUDE_CFLAGS = -g -O1 -DHYPRE_SEQUENTIAL=1 
CXXFLAGS :=  -g \
 -I..\
 -I../utilities\
 -I../IJ_mv\
 -I../seq_mv\
 -I../parcsr_mv\
 -I../parcsr_ls\
 -I../krylov\
 ${INCLUDE_CFLAGS}\
 -DHYPRE_TIMING

LFLAGSFP =\
 -L.\
 -L../parcsr_ls\
 -L../parcsr_mv\
 -L../IJ_mv\
 -L../seq_mv\
 -L../krylov\
 -L../utilities\
 -lHYPRE_utilities\
 ${INCLUDE_LFLAGS}\
 ${FPSANX_LFLAGS}

LFLAGS =\
 -L.\
 -L../parcsr_ls\
 -L../parcsr_mv\
 -L../IJ_mv\
 -L../seq_mv\
 -L../krylov\
 -L../utilities\
 -lparcsr_ls\
 -lparcsr_mv\
 -lseq_mv\
 -lIJ_mv\
 -lkrylov\
 -lHYPRE_utilities\
 -lm\
 ${INCLUDE_LFLAGS}

all: amg amg_fp

amg_fp: amg.fp.o
	@echo  "Linking" $@ "... "
	${CC} -o amg_fp amg.fp.o ${LFLAGSFP}

amg.fp.o: amg_opt_fp.bc
	llc $< -filetype=obj -o $@

amg_opt_fp.bc: amg_l.bc  
	$(OPT) <amg_l.bc> $@

amg_l.bc: $(OBJ_FILES_FP) ../parcsr_ls/*.fp.bc ../parcsr_mv/*.fp.bc ../IJ_mv/*.fp.bc ../seq_mv/*.fp.bc ../krylov/*.fp.bc 
	@echo  "Linking" $@ "... "
	llvm-link $^ -o $@ 

$(OBJ_DIR)/%.bc: $(SRC_DIR)/%.c
	$(CCE) $(CXXFLAGS) -c -o $@ $< 

amg: amg.o
	@echo  "Linking" $@ "... "
	${CC} -o amg amg.o ${LFLAGS}

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(GCC) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf  @rm -f amg amg_fp *.bc  *.out *.o *.dSYM *.bc seq_opt_fp
