#!/bin/bash

#build llvm and clang
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/llvm-10.0.0.src.tar.xz
tar -xvf llvm-10.0.0.src.tar.xz
mv llvm-10.0.0.src llvm
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang-10.0.0.src.tar.xz
tar -xvf clang-10.0.0.src.tar.xz
mv clang-10.0.0.src clang
mkdir build
cd build
cmake -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles" ../llvm
make -j
cd ..
export LLVM_HOME=$PWD/build

#build tbb
git clone https://github.com/wjakob/tbb.git
cd tbb/build
cmake ..
make -j
make DESTDIR=$PWD install
cd ../../
export TBB_HOME=$PWD/tbb

#build mpfr
wget https://www.mpfr.org/mpfr-current/mpfr-4.1.0.tar.xz
tar -xvf mpfr-4.1.0.tar.xz
cd mpfr-4.1.0
./configure --enable-thread-safe --prefix=$PWD
make
make install
cd ..
export MPFR_HOME="$PWD/mpfr-4.1.0"

#env variables
export CPP=clang++
export CC=clang
export PATH=$LLVM_HOME/bin:$PATH

