export CPP=clang++
export CC=clang

#export FPSAN_HOME=/home/sangeeta/fpsanitizer-1
export PFSAN_HOME=/common/home/sc1696/psan_fse_2021/
export FPSANX_HOME=/common/home/sc1696/FPSanX/FPSan-X
export LLVM_HOME=/common/home/sc1696/FPSanX/
export TBB_HOME=$FPSANX_HOME
export MPFR_HOME=/common/home/sc1696/mpfr/
export PATH=$LLVM_HOME/build/bin:$PATH
export LD_LIBRARY_PATH=$PFSAN_HOME/runtime/obj/:$TBB_HOME/build/
