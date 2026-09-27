#include <chrono>
#include <functional>
#include <memory>
#include <iostream>
#include <string>
#include <unistd.h>
#include "cli.hpp"
#include "context.hpp"
#include "ui.hpp"
#include "utils.hpp"
#include "file_ops.hpp"
#include "file_recent.hpp"
#include "file_dedup.hpp"
#include "file_near.hpp"
#include "file_watch.hpp"
#include "file_old.hpp"
#include "file_upload.hpp"
#include "file_history.hpp"
#include "journal.hpp"
#include "config.hpp"
#include "file_config.hpp"

#ifndef FILEMGR_VERSION
#define FILEMGR_VERSION "2.0.0-dev"
#endif

namespace {

struct Command {
    std::string name;
    std::string args;      // positional usage, e.g. "[n]"
    std::string summary;
    std::vector<OptionSpec> options;
    bool journaled;        // Moves are recorded so the run can be undone
    bool needs_root;       // Requires the managed folder to exist
    std::function<int(const Context&, const ParsedArgs&)> run;
};

const std::vector<OptionSpec> kGlobalOptions = {
    {"path", 'p', "DIR", "Manage DIR instead of ~/Downloads"},
    {"config", 'c', "FILE", "Read configuration from FILE"},
    {"dry-run", 'n', "", "Show what would happen without changing anything"},
    {"verbose", 'v', "", "Print extra detail"},
    {"quiet", 'q', "", "Only print errors and the final summary"},
    {"no-color", 0, "", "Disable colored output (also honours $NO_COLOR)"},
    {"time", 0, "", "Print how long the command took"},
    {"no-journal", 0, "", "Don't record moves (the run can't be undone)"},
    {"help", 'h', "", "Show help (for a command: filemgr <command> --help)"},
    {"version", 0, "", "Print version and exit"},
};

std::string positional(const ParsedArgs& args, size_t i, const std::string& fallback = "") {
    return i < args.positionals.size() ? args.positionals[i] : fallback;
}

fs::path configPath(const ParsedArgs& args) {
    return args.has("config") ? expandHome(args.get("config")) : defaultConfigPath();
}

// Managed folder precedence: --path, then $FILEMGR_ROOT, then config root, then ~/Downloads.
fs::path resolveRoot(const ParsedArgs& args, const Config& config) {
    fs::path root;
    if (args.has("path")) root = expandHome(args.get("path"));
    else if (const char* env = getenv("FILEMGR_ROOT"); env && *env) root = env;
    else if (!config.root.empty()) root = config.root;
    else root = downloadPath();
    root = fs::absolute(root).lexically_normal();
    if (root.filename().empty()) root = root.parent_path(); // drop trailing slash
    return root;
}

const std::vector<Command>& commands() {
    static const std::vector<Command> table = {
        {"sort", "", "Move top-level files into type folders (IMAGES, DOCUMENTS, ...)", {}, true, true,
         [](const Context& ctx, const ParsedArgs&) { return sortByType(ctx); }},
        {"recent", "[n]", "Bring the n most recently modified files back to the top level (default 5, see config)", {}, true, true,
         [](const Context& ctx, const ParsedArgs& a) {
             std::string fallback = std::to_string(ctx.config->recent_count);
             return recentFile(ctx, parseCount(positional(a, 0, fallback), "n"));
         }},
        {"dedup", "", "Move byte-identical duplicates into DUPLICATES/, keeping the oldest copy",
         {{"near", 0, "", "Also move visually similar images into DUPLICATES/NEAR/"},
          {"threshold", 't', "N", "Near-duplicate sensitivity, 0-64 bits (default 6; higher matches more)"}},
         true, true,
         [](const Context& ctx, const ParsedArgs& a) {
             int threshold = parseCount(a.get("threshold", std::to_string(kDefaultNearThreshold)), "--threshold");
             if (threshold > 64) throw UsageError("--threshold must be between 0 and 64");
             if (a.has("threshold") && !a.has("near")) throw UsageError("--threshold only applies with --near");
             int status = deduplicateFiles(ctx);
             if (status == 0 && a.has("near")) status = findNearDuplicates(ctx, threshold);
             return status;
         }},
        {"old", "[days]", "Archive files untouched for this many days into OLD/ (default 30, see config)", {}, true, true,
         [](const Context& ctx, const ParsedArgs& a) {
             std::string fallback = std::to_string(ctx.config->old_days);
             return archiveOld(ctx, parseCount(positional(a, 0, fallback), "days"));
         }},
        {"upload", "<folder>", "Upload a folder to Google Drive via rclone",
         {{"remote", 'r', "NAME", "rclone remote to upload to (default from config: gdrive)"}}, false, true,
         [](const Context& ctx, const ParsedArgs& a) {
             if (a.positionals.empty()) throw UsageError("upload requires a folder name");
             return uploadFolder(ctx, a.positionals[0], a.get("remote", ctx.config->remote));
         }},
        {"watch", "", "Sort new files automatically as they arrive (runs until Ctrl-C)",
         {{"settle", 's', "SECONDS", "Quiet period before a new file is sorted (default 2)"},
          {"sort-existing", 0, "", "Sort files already in the folder at startup"},
          {"print-plist", 0, "", "Print a launchd agent plist for running watch at login"}},
         true, true,
         [](const Context& ctx, const ParsedArgs& a) {
             if (a.has("print-plist")) return printLaunchdPlist(ctx);
             WatchOptions options;
             options.sort_existing = a.has("sort-existing");
             if (a.has("settle")) {
                 try {
                     options.settle_seconds = std::stod(a.get("settle"));
                 } catch (const std::exception&) {
                     options.settle_seconds = -1;
                 }
                 if (options.settle_seconds < 0 || options.settle_seconds > 3600)
                     throw UsageError("--settle must be a number of seconds between 0 and 3600");
             }
             return watchDownloads(ctx, options);
         }},
        {"history", "[id]", "List recent runs, or every move made by run <id>",
         {{"limit", 'l', "N", "Number of runs to list (default 15)"}}, false, false,
         [](const Context&, const ParsedArgs& a) {
             Journal journal(stateDirectory() / "journal");
             return showHistory(journal, parseCount(a.get("limit", "15"), "--limit"),
                                parseCount(positional(a, 0, "0"), "id"));
         }},
        {"undo", "[id]", "Reverse the last run (or run <id>), moving files back", {}, true, false,
         [](const Context& ctx, const ParsedArgs& a) {
             return undoRun(ctx, parseCount(positional(a, 0, "0"), "id"));
         }},
        {"config", "[show|path|init|edit]", "Show the effective configuration, or create/edit the config file",
         {{"force", 'f', "", "With init: overwrite an existing config file"}}, false, false,
         [](const Context& ctx, const ParsedArgs& a) {
             return configCommand(*ctx.config, configPath(a), positional(a, 0, "show"), a.has("force"));
         }},
    };
    return table;
}

const Command* findCommand(const std::string& name) {
    for (const auto& c : commands())
        if (c.name == name) return &c;
    return nullptr;
}

std::vector<OptionSpec> commandOptions(const std::string& name) {
    const Command* c = findCommand(name);
    return c ? c->options : std::vector<OptionSpec>{};
}

void printUsage() {
    std::cout << ui::bold("filemgr") << " " << FILEMGR_VERSION << " - keep your Downloads folder tidy\n\n"
              << ui::bold("Usage:") << " filemgr [options] <command> [args]\n\n"
              << ui::bold("Commands:") << "\n";
    for (const auto& c : commands()) {
        std::string left = c.name + (c.args.empty() ? "" : " " + c.args);
        std::cout << "  " << ui::cyan(left) << std::string(left.size() < 18 ? 18 - left.size() : 1, ' ')
                  << c.summary << "\n";
    }
    std::cout << "\n" << ui::bold("Options:") << "\n" << formatOptions(kGlobalOptions)
              << "\nRun 'filemgr <command> --help' for command-specific options.\n";
}

void printCommandHelp(const Command& c) {
    std::cout << ui::bold("Usage:") << " filemgr " << c.name << (c.args.empty() ? "" : " " + c.args)
              << " [options]\n\n" << c.summary << "\n";
    if (!c.options.empty()) {
        std::cout << "\n" << ui::bold("Options:") << "\n" << formatOptions(c.options);
    }
    std::cout << "\n" << ui::bold("Global options:") << "\n" << formatOptions(kGlobalOptions);
}

bool shouldUseColor(const ParsedArgs& args) {
    if (args.has("no-color") || getenv("NO_COLOR")) return false;
    return isatty(STDOUT_FILENO);
}

} // namespace

int main(int argc, char* argv[]) {
    auto start_time = std::chrono::steady_clock::now();

    ParsedArgs args;
    try {
        args = parseArgs(argc, argv, kGlobalOptions, commandOptions);
    } catch (const UsageError& e) {
        ui::error(e.what());
        std::cerr << "Run 'filemgr --help' for usage.\n";
        return 2;
    }

    ui::Verbosity verbosity = ui::Verbosity::Normal;
    if (args.has("quiet")) verbosity = ui::Verbosity::Quiet;
    if (args.has("verbose")) verbosity = ui::Verbosity::Verbose;
    ui::configure(verbosity, shouldUseColor(args));

    if (args.has("version")) {
        std::cout << "filemgr " << FILEMGR_VERSION << "\n";
        return 0;
    }

    if (args.command == "help") {
        const Command* c = findCommand(positional(args, 0));
        if (c) printCommandHelp(*c); else printUsage();
        return 0;
    }

    if (args.command.empty()) {
        printUsage();
        return args.has("help") ? 0 : 1;
    }

    const Command* command = findCommand(args.command);
    if (!command) {
        ui::error("unknown command '" + args.command + "'");
        std::cerr << "Run 'filemgr --help' for the list of commands.\n";
        return 2;
    }
    if (args.has("help")) {
        printCommandHelp(*command);
        return 0;
    }

    int status = 0;
    try {
        Config config = loadConfig(configPath(args));
        filemgr_directories = config.managedFolders();

        Context ctx;
        ctx.config = &config;
        ctx.root = resolveRoot(args, config);
        ctx.dry_run = args.has("dry-run");

        // Journal every mutating command so it can be undone later. undo is
        // journaled too, which makes "undo the undo" a redo.
        std::unique_ptr<Journal> journal;
        if (command->journaled && (!args.has("no-journal") || command->name == "undo")) {
            journal = std::make_unique<Journal>(stateDirectory() / "journal");
            std::string invocation = command->name;
            for (const auto& p : args.positionals) invocation += " " + p;
            journal->beginRun(invocation, ctx.root);
            ctx.journal = journal.get();
        }

        if (command->needs_root && !fs::is_directory(ctx.root)) {
            ui::error(ctx.root.string() + " is not a directory");
            return 1;
        }
        if (ctx.dry_run) {
            ui::info(ui::yellow("Dry run: no files will be changed."));
        }
        status = command->run(ctx, args);
    } catch (const UsageError& e) {
        ui::error(e.what());
        std::cerr << "Run 'filemgr " << command->name << " --help' for usage.\n";
        return 2;
    } catch (const ConfigError& e) {
        ui::error(std::string("in config: ") + e.what());
        return 1;
    } catch (const std::exception& e) {
        ui::error(e.what());
        return 1;
    }

    if (args.has("time") || ui::verbose()) {
        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
        std::cout << ui::dim("Execution time: " + std::to_string(elapsed.count()) + " seconds") << "\n";
    }
    return status;
}
