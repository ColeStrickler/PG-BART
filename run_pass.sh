#!/bin/bash
set -e

if [ $# -ne 4 ]; then
    echo "Usage: $0 <pass1> <pass2> <testfile.cpp> <runtime.cpp>"
    echo "Example: $0 InstructionAnalysisPass MemoryAnalysisPass matmul.cpp online_analyzer.cpp"
    exit 1
fi

PASS1="$1"
PASS2="$2"
TESTFILE="$3"
RUNTIME="$4"

BASE="${TESTFILE%.*}"
BC_FILE="${BASE}.bc"

CURR_DIR=$(pwd)
PASS1_DIR="./pass/$PASS1"
PASS2_DIR="./pass/$PASS2"
TEST_FILE="./test/$TESTFILE"

LLVM_PREFIX=$(llvm-config --prefix)

# Debug output
echo "=== Debug Info ==="
echo "PASS1_DIR = $PASS1_DIR"
echo "PASS2_DIR = $PASS2_DIR"
ls -la ./pass/

# Checks
[ ! -d "$PASS1_DIR" ] && { echo "Error: $PASS1_DIR not found"; exit 1; }
[ ! -d "$PASS2_DIR" ] && { echo "Error: $PASS2_DIR not found"; exit 1; }
[ ! -f "$TEST_FILE" ] && { echo "Error: Test file not found"; exit 1; }

# Build Pass 1
echo "=== Building $PASS1 ==="
cd "$PASS1_DIR"
rm -rf build && mkdir -p build && cd build
cmake .. -DLLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Build Pass 2
echo "=== Building $PASS2 ==="
cd "$CURR_DIR"
cd "$PASS2_DIR"
rm -rf build && mkdir -p build && cd build
cmake .. -DLLVM_DIR="$LLVM_PREFIX/lib/cmake/llvm" -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

cd "$CURR_DIR"

# Rest of the script (compile → opt → link) ...
clang++ -O3 -fno-inline -fno-inline-functions -g -emit-llvm -c "test/$TESTFILE" -o "test/$BC_FILE"

opt \
    -load-pass-plugin "$PASS1_DIR/build/lib$PASS1.so" \
    -load-pass-plugin "$PASS2_DIR/build/lib$PASS2.so" \
    -passes="$PASS1,$PASS2" \
    "test/$BC_FILE" -o "test/instrumented.bc"

clang++ -O3 -g test/instrumented.bc \
    "src/$RUNTIME" src/base64.cpp src/elf_info.cpp src/online_analyzer.cpp src/cache.cpp \
    -lelf -o test/a.out

echo "=== Running ==="
./test/a.out