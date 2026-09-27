#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>
#include "cli.hpp"
#include "commands.hpp"
#include "config.hpp"
#include "context.hpp"
#include "journal.hpp"
#include "ui.hpp"
#include "utils.hpp"

#ifndef FILEMGR_VERSION
#define FILEMGR_VERSION "2.0.0-dev"
#endif

namespace {

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

void printUsage() {
    std::cout << ui::bold("filemgr") << " " << FILEMGR_VERSION << " - keep your Downloads folder tidy\n\n"
              << ui::bold("Usage:") << " filemgr [options] <command> [args]\n\n"
              << ui::bold("Commands:") << "\n";
    std::size_t width = 0;
    for (const auto& c : commands()) width = std::max(width, c.name.size() + 1 + c.args.size());
    for (const auto& c : commands()) {
        std::string left = c.name + (c.args.empty() ? "" : " " + c.args);
        std::cout << "  " << ui::cyan(left) << std::string(width + 2 - left.size(), ' ') << c.summary << "\n";
    }
    std::cout << "\n" << ui::bold("Options:") << "\n" << formatOptions(globalOptions())
              << "\nRun 'filemgr <command> --help' for command-specific options.\n";
}

void printCommandHelp(const Command& c) {
    std::cout << ui::bold("Usage:") << " filemgr " << c.name << (c.args.empty() ? "" : " " + c.args)
              << " [options]\n\n" << c.summary << "\n";
    if (!c.options.empty()) {
        std::cout << "\n" << ui::bold("Options:") << "\n" << formatOptions(c.options);
    }
    std::cout << "\n" << ui::bold("Global options:") << "\n" << formatOptions(globalOptions());
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
        args = parseArgs(argc, argv, globalOptions(), commandOptions);
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
