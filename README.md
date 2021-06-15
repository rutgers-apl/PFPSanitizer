# PFPSanitizer - A Parallel Shadow Execution Tool for Debugging Numerical Errors 

[Work in Progress] Do not use right now.


 Please note that we have disabled turbo-boost and hyper-threading and our data is 
 generated on a 64 core machine. Generated graphs in step (5) would not match if you
 are not using the 64 core machine. 

(1). Install the prerequisites:
```
  $ apt-get update
  $ apt-get install -yq build-essential cmake git jgraph libgmp3-dev libmpfr-dev python2 python3 wget
```
(2). Get PFPSanitizer ready

(a). Clone fpsan git repo
```
  $ git clone git@github.com:rutgers-apl/PFPSanitizer.git
  $ cd PFPSanitizer
  $ . ./build_prereq.sh 
```
(b). Compile PFPSanitizer pass

If your compiler does not support C++11 by default, add the following line to llvm-pass/FPSan/CMakefile
```
target_compile_feature(FPSanitizer PRIVATE cxx_range_for cxx_auto_type)
```
otherwise, use the followng line
```
 target_compile_features(FPSanitizer PRIVATE )
```
```
  $ cd llvm-pass
  $ mkdir build
  $ cd build
  $ cmake ../
  $ make
  $ cd ../..
```

(c). Build the PFPSanitizer runtime environment
```
  $ cd runtime 
  $ make
  $ cd ..
```

(d). Set runtime env variable
```
  $ export PFSAN_HOME=<path to PFPSanitizer directory(/PFPSanitizer)>
  $ export LD_LIBRARY_PATH=$PFSAN_HOME/runtime/obj/:$TBB_HOME/build/
```

(3). Correctness testing(Section 5: (Ability to detect FP errors))
```
  $ cd correctness_suite
  $ python3 correctness_test.py
  $ cd ..
```
  This script will run microbenchmarks with PFPSanitizer and report numerical errors. 
  It runs total of 43 benchmarks and report numerical errors in these benchmarks, ie, 
  catastrophic cancellation, NaN, Inf, branch flips and integer conversion error.

(4). Debugging (Section 5: (Debugging a previously unknown error in Cholesky from Polybench))

To run cholesky with gdb, compile runtime with O0
```
 $ export SET_DEBUG=DEBUG //reset to debug mode
 $ cd runtime
 $ make clean
 $ make
 $ cd ../case_study/cholesky
 $ make clean
 $ make
 $ gdb ./cholesky_fp
 $ b handleReal.cpp:1335
   Make breakpoint pending on future shared library load? (y or [n]) y
 $ r
 $ call fpsan_trace(buf_id, res)
``` 
 This will show the error trace matching with Figure 7(a) in the paper. 
 This trace shows the "Inst ID: Opcode: Op1_Inst_ID: Op2_Inst_ID: (real value: computed value: error)"
 As it can be seen in the trace that Inf exception has occured in instruction 189.
 If you trace back, then you would notice that error was propagated from the instruction 450.
 However, instruction 450 is computed in the different function. To trace back the error in instruction
 450 follow the below instructions.

```
 $ b handleReal.cpp:1287 if res->error >= 28
 $ r
   Start it from the beginning? (y or n) y
 $ call fpsan_trace(buf_id, res)
```
 You will get the trace matching Figure 7(b).
 This error trace would show that addition of 1 and 2.70400000000000e7 has resulted in error of 28 bits.
 First instruction in the trace shows real computation as 2.70400010000000e7 and floating point compuation 
 as 27040000, hence rounding error has occured.

(5). Performance testing (Section 5: (Performance speedup with PFPSanitizer compared to FPSanitizer))
```
  $ cd PFPSanitizer/performance
  $ ./run_perf.sh
  This script will run peformance benchmarks and produce graphs speedup.pdf(Figure 8) 
  and slowdown.pdf(Figure 9). 
  Please note that we have disabled turbo-boost and hyper-threading and our data is 
  generated on a 64 core machine. 
```
