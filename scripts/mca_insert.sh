#!/bin/bash

set -e

PASS="$1"
TESTFILE="$2"

PASS_DIR="pass/$PASS"
LLVM_PREFIX=$(llvm-config --prefix)

BASE="${TESTFILE%.*}"
BC_FILE="${BASE}.bc"
INSTR_BC="${BASE}.instrumented.bc"

pwd

echo "=== Building LLVM pass ==="
cd "$PASS_DIR"
rm -rf build && mkdir -p build && cd build
cmake .. -DLLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ../../../

clang++ -O3 -fno-inline -fno-inline-functions -g -emit-llvm -c "test/$TESTFILE" -o "test/$BC_FILE"

echo "=== Running LLVM pass ==="

opt \
  -load-pass-plugin "$PASS_DIR/build/lib$PASS.so" \
  -passes="$PASS" \
  "test/$BC_FILE" \
  -o "test/$INSTR_BC"


llc -O3 -mcpu=native -filetype=asm \
    "test/$BASE.instrumented.bc" -o "test/$BASE.s"
# 2. Run llvm-mca with markers enabled
llvm-mca -mcpu=native \
         --timeline \
         --iterations=500 \
         --dispatch-stats \
         "test/$BASE.s" > output_artifacts/mca_report.txt

#echo "=== Running binary ==="
#clang++ "test/$INSTR_BC" -O3 -g -o "test/${BASE}.out"
#./test/${BASE}.out