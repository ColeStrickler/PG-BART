#!/bin/bash

pass="$1"
testfile="$2"
runtime_file="$3"
base="${testfile%.*}"
bc_file="$base.bc"

curr_dir=$(pwd)
pass_dir="./pass/$pass"
test_file="./test/$testfile"
llvm_prefix=$(llvm-config --prefix)

if [ ! -d "$pass_dir" ]; then
  echo "Error: pass directory does not exist"
  exit 1
fi

if [ ! -f "$test_file" ]; then
  echo "Error: test code file does not exist"
  exit 1
fi

cd "$pass_dir"
rm -rf build
mkdir -p build
cd build

cmake .. -DLLVM_DIR="$llvm_prefix/lib/cmake/llvm"
make

cd "$curr_dir"


clang++ -O3 -g  -emit-llvm -c "test/$testfile" -o "test/$bc_file"
opt -load-pass-plugin "$pass_dir/build/lib$pass.so" \
    -passes="$pass" \
    "test/$bc_file" \
    -o "test/instrumented.bc"
clang++ test/instrumented.bc src/$runtime_file src/base64.cpp src/elf_info.cpp -lelf -o test/a.out
./test/a.out
