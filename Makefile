
# =============================================
# Makefile for PG-BART Offline Analyzer
# =============================================

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -g
LDFLAGS = 

# Directories
SRC_DIR = src
BUILD_DIR = build
TARGET = offline_analyzer

# Source files for offline_analyzer
SRCS = $(SRC_DIR)/offline_analzyer.cpp \
       $(SRC_DIR)/cache.cpp

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
	./$(TARGET)

# Clean build artifacts
clean:
	rm $(BUILD_DIR)/* $(TARGET)
	@echo "Cleaned build artifacts"

# Rebuild from scratch
rebuild: clean all

.PHONY: all clean rebuild run