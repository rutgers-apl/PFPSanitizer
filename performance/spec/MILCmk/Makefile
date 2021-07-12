CC = clang -pg -ffast-math -g
CCE = clang -pg -fno-inline -ffast-math -emit-llvm -g
#COPT = -O3 -qhot -qsimd=noauto
COPT = -O1 
OPT = opt -load $(PFSAN_HOME)/llvm-pass/build/FPSan/libFPSanitizer.so -fpsan
CFLAGS= -fno-slp-vectorize -std=c11 -L$(PFSAN_HOME)/runtime/obj -lfpsanitizer  -L$(MPFR_HOME)/src/.libs/ -lmpfr -lm  -lstdc++ -L$(TBB_HOME)/build -ltbb -I$(MPFR_HOME)/include
QLADIR = qla-1.7.1
CINC = -I$(QLADIR)
#CFLAGSA = -DHAVE_XLC
CFLAGSF = $(CFLAGSA) $(COPT) $(OMP) $(CINC) -DQLA_Precision=\'F\' -DQLA_Colors=3 -DQLA_Nc=3 
CFLAGSD = $(CFLAGSA) $(COPT) $(OMP) $(CINC) -DQLA_Precision=\'D\' -DQLA_Colors=3 -DQLA_Nc=3 

FSRC = $(wildcard $(QLADIR)/QLA_D3_*.c)
FOBJS = $(patsubst %.c, %.bc, $(FSRC))
DOBJS = $(patsubst %.c, %.o, $(FSRC))

PROGS = qla_bench-$(QLADIR)-f3 qla_bench-$(QLADIR)-f3_fp

all: $(PROGS)

qla_bench-$(QLADIR)-f3_fp: $(QLADIR)/qla_f_fp.o
	$(CC) $(CFLAGSD) $< -o $@ $(CFLAGS)

qla_bench-$(QLADIR)-f3: qla_bench.c $(DOBJS)
	$(CC) $(CFLAGSD) $< $(DOBJS) -o $@ $(CFLAGS)

$(QLADIR)/qla_f_fp.o:$(QLADIR)/qla_f_opt.bc
	llc $< -filetype=obj -o $@

$(QLADIR)/qla_f_opt.bc:$(QLADIR)/qla_f.bc
	$(OPT) <$(QLADIR)/qla_f.bc> $@ 

$(QLADIR)/qla_f.bc:$(FOBJS) qla_bench.bc
	llvm-link $^ -o $@ 

qla_bench.bc:%.bc:%.c
	$(CCE) $(CFLAGSD) -c $< -o $@

$(FOBJS):%.bc:%.c
	$(CCE) $(CFLAGSD) -c $< -o $@

$(DOBJS):%.o:%.c
	$(CC) $(CFLAGSD) -c $< -o $@

clean:
	rm -f $(PROGS) *.o $(QLADIR)/*.o $(QLADIR)/*.bc *.bc *.o

tarball:
	tar jcvhf MILCmk-v1.tar.bz2 --transform 's|^./|MILCmk-v1/|' ./README ./Makefile ./runtests ./bench-*.output ./*.[ch] ./qla-1.7.1/*.[ch]
