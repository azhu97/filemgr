#pragma once
#include <string>
#include "config.hpp"

// `filemgr completions <shell>`: prints a zsh, bash or fish completion script
// generated from the command table, so completions never drift from the CLI.
// Category names are taken from `config`; rule names are looked up live
// (`filemgr clean --names`) each time you press Tab.
int printCompletions(const std::string& shell, const Config& config);
