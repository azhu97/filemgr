#include "cli.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

std::string ParsedArgs::get(const std::string& name, const std::string& fallback) const {
    auto it = options.find(name);
    return it == options.end() ? fallback : it->second;
}

namespace {
const OptionSpec* findLong(const std::vector<OptionSpec>& specs, const std::string& name) {
    for (const auto& s : specs)
        if (s.long_name == name) return &s;
    return nullptr;
}

const OptionSpec* findShort(const std::vector<OptionSpec>& specs, char c) {
    for (const auto& s : specs)
        if (s.short_name != 0 && s.short_name == c) return &s;
    return nullptr;
}
} // namespace

ParsedArgs parseArgs(int argc, char* argv[],
                     const std::vector<OptionSpec>& globalOptions,
                     std::vector<OptionSpec> (*commandOptions)(const std::string&)) {
    ParsedArgs result;
    std::vector<OptionSpec> specs = globalOptions;
    bool options_done = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (options_done || arg.size() < 2 || arg[0] != '-') {
            if (result.command.empty()) {
                result.command = arg;
                auto extra = commandOptions(arg);
                specs.insert(specs.end(), extra.begin(), extra.end());
            } else {
                result.positionals.push_back(arg);
            }
            continue;
        }

        if (arg == "--") {
            options_done = true;
            continue;
        }

        const OptionSpec* spec = nullptr;
        std::string value;
        bool has_inline_value = false;

        if (arg.rfind("--", 0) == 0) {
            std::string name = arg.substr(2);
            auto eq = name.find('=');
            if (eq != std::string::npos) {
                value = name.substr(eq + 1);
                name = name.substr(0, eq);
                has_inline_value = true;
            }
            spec = findLong(specs, name);
            if (!spec) throw UsageError("unknown option --" + name);
        } else {
            if (arg.size() != 2) throw UsageError("unknown option " + arg);
            spec = findShort(specs, arg[1]);
            if (!spec) throw UsageError("unknown option " + arg);
        }

        if (spec->value_name.empty()) {
            if (has_inline_value) throw UsageError("--" + spec->long_name + " does not take a value");
        } else if (!has_inline_value) {
            if (i + 1 >= argc) throw UsageError("--" + spec->long_name + " requires " + spec->value_name);
            value = argv[++i];
        }
        result.options[spec->long_name] = value;
    }
    return result;
}

int parseCount(const std::string& text, const std::string& what) {
    if (text.empty() || text.size() > 9 ||
        !std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); })) {
        throw UsageError(what + " must be a non-negative whole number, got '" + text + "'");
    }
    return std::stoi(text);
}

std::string formatOptions(const std::vector<OptionSpec>& options) {
    std::vector<std::string> left;
    size_t width = 0;
    for (const auto& o : options) {
        std::string l = o.short_name ? std::string("-") + o.short_name + ", " : "    ";
        l += "--" + o.long_name;
        if (!o.value_name.empty()) l += " " + o.value_name;
        width = std::max(width, l.size());
        left.push_back(l);
    }
    std::ostringstream out;
    for (size_t i = 0; i < options.size(); ++i) {
        out << "  " << left[i] << std::string(width - left[i].size() + 2, ' ') << options[i].help << "\n";
    }
    return out.str();
}
