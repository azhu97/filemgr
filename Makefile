# Simple Makefile for filemgr
CXX := c++
CXXFLAGS := -std=c++17 -Wall -Iinclude

SRCDIR := src
INCLUDEDUR := include
OBJDIR := build
SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
TARGET := filemgr

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

file_dedup:
	$(CXX) $(CXXFLAGS) $(SRCDIR)/file_dedup.cpp $(SRCDIR)/file_ops.cpp -I $(INCLUDEDUR) -o file_dedup
