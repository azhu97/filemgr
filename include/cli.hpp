#pragma once
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Minimal GNU-style argument parser: --long, --long=value, --long value,
// -s, -s value, and "--" to end option parsing. Options may appear before
// or after the command name.

struct UsageError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct OptionSpec {
    std::string long_name;   // e.g. "dry-run"
    char short_name;         // e.g. 'n', or 0 for none
    std::string value_name;  // e.g. "DIR"; empty for boolean flags
    std::string help;
};

struct ParsedArgs {
    std::string command;
    std::vector<std::string> positionals;
    std::map<std::string, std::string> options;  // long_name -> value ("" for flags)

    bool has(const std::string& name) const { return options.count(name) > 0; }
    std::string get(const std::string& name, const std::string& fallback = "") const;
};

// `commandOptions` returns the extra options a given command accepts
// (called once the command name is known).
ParsedArgs parseArgs(int argc, char* argv[],
                     const std::vector<OptionSpec>& globalOptions,
                     std::vector<OptionSpec> (*commandOptions)(const std::string&));

// Parses a non-negative integer, throwing UsageError with `what` on failure.
int parseCount(const std::string& text, const std::string& what);

// Formats a list of options as aligned help text.
std::string formatOptions(const std::vector<OptionSpec>& options);
