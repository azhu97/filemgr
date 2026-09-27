#include "commands.hpp"
#include "completions.hpp"
#include "config.hpp"
#include "file_clean.hpp"
#include "file_config.hpp"
#include "file_dedup.hpp"
#include "file_find.hpp"
#include "file_history.hpp"
#include "file_near.hpp"
#include "file_old.hpp"
#include "file_ops.hpp"
#include "file_recent.hpp"
#include "file_upload.hpp"
#include "file_watch.hpp"
#include "filter.hpp"
#include "journal.hpp"
#include "stats.hpp"

const std::vector<OptionSpec>& globalOptions() {
    static const std::vector<OptionSpec> options = {
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
    return options;
}

std::string positional(const ParsedArgs& args, size_t i, const std::string& fallback) {
    return i < args.positionals.size() ? args.positionals[i] : fallback;
}

fs::path configPath(const ParsedArgs& args) {
    return args.has("config") ? expandHome(args.get("config")) : defaultConfigPath();
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
        {"clean", "[rule...]", "Apply the cleanup rules from your config (all enabled rules, or those named)",
         {{"list", 0, "", "List rules and how many files each matches right now"},
          {"names", 0, "", "Print rule names only, one per line (for scripts)"}},
         true, true,
         [](const Context& ctx, const ParsedArgs& a) {
             if (a.has("names")) {
                 for (const auto& r : ctx.config->rules) std::cout << r.name << "\n";
                 return 0;
             }
             return cleanWithRules(ctx, a.positionals, a.has("list"));
         }},
        {"completions", "<shell>", "Print a completion script for zsh, bash or fish",
         {}, false, false,
         [](const Context& ctx, const ParsedArgs& a) {
             if (a.positionals.empty()) throw UsageError("completions requires a shell: zsh, bash or fish");
             return printCompletions(a.positionals[0], *ctx.config);
         }},
        {"stats", "", "Report disk usage by type, folder and age, plus cleanup opportunities",
         {{"top", 0, "N", "Number of largest files to list (default 10)"},
          {"no-dups", 0, "", "Skip the duplicate scan (faster on huge folders)"},
          {"html", 0, "FILE", "Write a standalone HTML report to FILE instead"}},
         false, true,
         [](const Context& ctx, const ParsedArgs& a) {
             return showStats(ctx, parseCount(a.get("top", "10"), "--top"), !a.has("no-dups"), a.get("html"));
         }},
        {"find", "[pattern...]", "Search files by name, type, size and age (read-only)",
         {{"type", 'T', "CATEGORY", "Only files in this category (IMAGES, VIDEOS, ..., Other)"},
          {"ext", 'e', "LIST", "Only these extensions, e.g. jpg,png"},
          {"larger", 0, "SIZE", "At least SIZE, e.g. 100M"},
          {"smaller", 0, "SIZE", "At most SIZE, e.g. 10K"},
          {"older", 0, "AGE", "Last modified at least AGE ago, e.g. 30d, 6m, 1y"},
          {"newer", 0, "AGE", "Last modified within AGE, e.g. 12h, 2w"},
          {"in", 0, "FOLDER", "Search only this subfolder"},
          {"sort", 0, "KEY", "date (default, newest first), size or name"},
          {"limit", 'l', "N", "Show at most N results"},
          {"paths", 0, "", "Print bare absolute paths (for piping)"},
          {"print0", '0', "", "Like --paths, NUL-separated (for xargs -0)"},
          {"hidden", 0, "", "Include dotfiles and hidden folders"}},
         false, true,
         [](const Context& ctx, const ParsedArgs& a) {
             FindOptions o;
             try {
                 o.filter.names = a.positionals;
                 if (a.has("ext")) o.filter.addExtensions(a.get("ext"));
                 o.filter.category = a.get("type");
                 if (a.has("larger")) o.filter.min_size = parseSize(a.get("larger"));
                 if (a.has("smaller")) o.filter.max_size = parseSize(a.get("smaller"));
                 if (a.has("older")) o.filter.min_age = parseAge(a.get("older"));
                 if (a.has("newer")) o.filter.max_age = parseAge(a.get("newer"));
             } catch (const FilterError& e) {
                 throw UsageError(e.what());
             }
             o.within = a.get("in");
             o.sort = a.get("sort", "date");
             if (o.sort != "date" && o.sort != "size" && o.sort != "name")
                 throw UsageError("--sort must be date, size or name");
             o.limit = parseCount(a.get("limit", "0"), "--limit");
             o.null_separated = a.has("print0");
             o.paths_only = a.has("paths") || o.null_separated;
             o.include_hidden = a.has("hidden");
             return findFiles(ctx, o);
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
        {"config", "[action]", "Show the effective config, or: path, init, edit",
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
