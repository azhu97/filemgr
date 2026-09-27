# filemgr build
#   make            build ./filemgr
#   make install    install binary, man page and shell completions under $(PREFIX) (default ~/.local)
#   make test       build and run unit + end-to-end tests
#   make clean      remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CPPFLAGS += -Iinclude -MMD -MP
LDFLAGS  += -framework CoreFoundation -framework Security -framework CoreGraphics -framework ImageIO -framework CoreServices

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
	mkdir -p $(PREFIX)/bin $(PREFIX)/share/man/man1 \
	         $(PREFIX)/share/zsh/site-functions $(PREFIX)/share/bash-completion/completions \
	         $(PREFIX)/share/fish/vendor_completions.d
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	install -m 644 man/filemgr.1 $(PREFIX)/share/man/man1/filemgr.1
	./$(TARGET) completions zsh  > $(PREFIX)/share/zsh/site-functions/_filemgr
	./$(TARGET) completions bash > $(PREFIX)/share/bash-completion/completions/filemgr
	./$(TARGET) completions fish > $(PREFIX)/share/fish/vendor_completions.d/filemgr.fish

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET) $(PREFIX)/share/man/man1/filemgr.1 \
	      $(PREFIX)/share/zsh/site-functions/_filemgr \
	      $(PREFIX)/share/bash-completion/completions/filemgr \
	      $(PREFIX)/share/fish/vendor_completions.d/filemgr.fish

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(DEPS)

.PHONY: all install uninstall clean test
