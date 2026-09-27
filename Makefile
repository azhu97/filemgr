# filemgr build
#   make            build ./filemgr
#   make install    install to $(PREFIX)/bin (default ~/.local)
#   make test       build and run unit + end-to-end tests
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

TEST_SOURCES := $(wildcard tests/*.cpp)
TEST_OBJECTS := $(TEST_SOURCES:tests/%.cpp=$(BUILD)/tests/%.o)
LIB_OBJECTS  := $(filter-out $(BUILD)/main.o,$(OBJECTS))
TEST_BIN     := $(BUILD)/unit_tests

DEPS     := $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/tests/%.o: tests/%.cpp | $(BUILD)
	@mkdir -p $(BUILD)/tests
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -Itests -c $< -o $@

$(BUILD):
	mkdir -p $@

$(TEST_BIN): $(LIB_OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test: $(TARGET) $(TEST_BIN)
	@echo "== unit tests =="
	@./$(TEST_BIN)
	@echo "== end-to-end tests =="
	@tests/e2e.sh ./$(TARGET)

install: $(TARGET)
	mkdir -p $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(DEPS)

.PHONY: all install uninstall clean test
