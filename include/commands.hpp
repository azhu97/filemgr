#pragma once
#include <functional>
#include <string>
#include <vector>
#include "cli.hpp"
#include "context.hpp"

// The command table: every subcommand's name, usage, options and handler.
// main.cpp dispatches through it and the help text and shell completions are
// generated from it, so adding a command here is enough to expose it everywhere.

struct Command {
    std::string name;
    std::string args;      // positional usage, e.g. "[n]"
    std::string summary;
    std::vector<OptionSpec> options;
    bool journaled;        // Moves are recorded so the run can be undone
    bool needs_root;       // Requires the managed folder to exist
    std::function<int(const Context&, const ParsedArgs&)> run;
};

const std::vector<OptionSpec>& globalOptions();
const std::vector<Command>& commands();
const Command* findCommand(const std::string& name);
std::vector<OptionSpec> commandOptions(const std::string& name);

// Positional argument i, or `fallback` when absent.
std::string positional(const ParsedArgs& args, size_t i, const std::string& fallback = "");

// --config FILE, else the default config location.
fs::path configPath(const ParsedArgs& args);
