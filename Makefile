
# =============================================
# Makefile for PG-BART Offline Analyzer
# =============================================

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -g
LDFLAGS = -lelf

# Directories
SRC_DIR = src
BUILD_DIR = build
TARGET = offline_analyzer

# Source files for offline_analyzer
SRCS = $(SRC_DIR)/offline_analzyer.cpp \
       $(SRC_DIR)/cache.cpp \
	   $(SRC_DIR)/base64.cpp \
	   $(SRC_DIR)/elf_info.cpp

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $@

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp 
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link the executable
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@
	@echo "Build successful: $@"

# Run the analyzer
run: $(TARGET)
	./$(TARGET) output_artifacts/InstructionAnalysis.csv


inst: $(TARGET)
	./$(TARGET) output_artifacts/InstructionAnalysis.csv

mem: $(TARGET)
	./$(TARGET) output_artifacts/MemoryAnalysis.csv


matmul: test/matmul.cpp
	g++ -O3 test/matmul.cpp -o build/matmul
	chmod +x build/matmul
	./build/matmul

# Clean build artifacts
clean:
	rm $(BUILD_DIR)/* $(TARGET)
	@echo "Cleaned build artifacts"


mca:
	clang++ -shared -fPIC -fno-rtti -fno-exceptions \
		-I/home/cole/Documents/llvm-project/clang/include \
		-I/home/cole/Documents/llvm-project/build/include \
		-I/home/cole/Documents/llvm-project/build/tools/clang/include \
		-I/home/cole/Documents/llvm-project/llvm/include \
		plugin/MCAInserter.cpp \
		-o MCAInserter.so

# Rebuild from scratch
rebuild: clean all

.PHONY: all clean rebuild run