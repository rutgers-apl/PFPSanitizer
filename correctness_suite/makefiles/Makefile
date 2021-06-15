#CFLAGS=-O1 -fno-slp-vectorize -std=c11  -L$(FPROOT)/obj -lfpsanitizer -lmpfr -lm -pthread -ltbb -lstdc++
CFLAGS=-O0 -g -std=c11 -L$(PFSAN_HOME)/runtime/obj -lfpsanitizer -L$(MPFR_HOME)/src/.libs -lmpfr -lm  -lpthread -L$(TBB_HOME)/build  -ltbb 
OPT = opt -load $(PFSAN_HOME)/llvm-pass/build/FPSan/libFPSanitizer.so -fpsan
CSRC = $(wildcard *.c)
COUT = $(patsubst %.c,%.c.bc,$(CSRC))
OCOUT = $(patsubst %.c.bc,%.opt.bc,$(COUT))
OUT = $(patsubst %.opt.bc,%.c.o,$(OCOUT))
FOUT = $(patsubst %.c.o,%.fp.o,$(OUT))

CC = clang -pg -O0
CCE = clang -pg -O0 -fno-inline -emit-llvm

all: $(FOUT) 

.PRECIOUS: %.opt.bc

%.fp.o: %.c.o
	$(CC) $^ -o $@  $(CFLAGS) 
	chmod u+x $@

%.c.o: %.opt.bc
	llc $< -filetype=obj -o $@  

%.opt.bc: %.c.bc
	$(OPT) < $< > $@

%.c.bc: %.c
	$(CCE) -c -o $@ $< 

clean:
	rm -rf *.ll *.bc *.o *.sout *.dSYM *~ *.log functions.txt  *.out
