# filemgr build
#   make            build ./filemgr
#   make install    install to $(PREFIX)/bin (default ~/.local)
#   make clean      remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CPPFLAGS += -Iinclude -MMD -MP
LDFLAGS  += -framework CoreFoundation -framework Security

PREFIX   ?= $(HOME)/.local
BUILD    := build
TARGET   := filemgr

SOURCES  := $(wildcard src/*.cpp)
OBJECTS  := $(SOURCES:src/%.cpp=$(BUILD)/%.o)
DEPS     := $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

install: $(TARGET)
	mkdir -p $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(DEPS)

.PHONY: all install uninstall clean
