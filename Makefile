# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -framework CoreFoundation -framework Security

# Directories
SRC_DIR = .
OBJ_DIR = obj
BIN_DIR = bin

# Files
SOURCES = deduplicate.cpp
OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/dedupe

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJECTS)
	mkdir -p $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Install to system
install: $(TARGET)
	mkdir -p $(HOME)/.local/bin
	cp $(TARGET) $(HOME)/.local/bin/dedupe
	chmod +x $(HOME)/.local/bin/dedupe

.PHONY: all clean install