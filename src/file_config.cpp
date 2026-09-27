#include "file_config.hpp"
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace {

int showConfig(const Config& config, const fs::path& config_path) {
    ui::info(ui::bold("Config file: ") + config_path.string() +
             (config.source.empty() ? ui::dim("  (not found, using defaults)") : ""));
    ui::info("");
    ui::info(ui::bold("[general]"));
    ui::info("  root         = " + (config.root.empty() ? ui::dim("~/Downloads (default)") : config.root.string()));
    ui::info("  remote       = " + config.remote);
    ui::info("  old_days     = " + std::to_string(config.old_days));
    ui::info("  recent_count = " + std::to_string(config.recent_count));
    ui::info("  threads      = " + (config.threads ? std::to_string(config.threads) : "auto"));
    ui::info("");
    ui::info(ui::bold("[categories]"));

    // Show each extension under the folder it actually maps to (later categories win).
    auto map = config.extensionMap();
    for (const auto& c : config.categories) {
        std::string exts;
        for (const auto& e : c.extensions) {
            if (map[e] != c.folder) continue;
            exts += (exts.empty() ? "" : " ") + e.substr(1);
        }
        ui::info("  " + ui::cyan(c.folder) + std::string(c.folder.size() < 12 ? 12 - c.folder.size() : 1, ' ') +
                 "= " + (exts.empty() ? ui::dim("(none)") : exts));
    }
    return 0;
}

int initConfig(const fs::path& config_path, bool force) {
    if (fs::exists(config_path) && !force) {
        ui::error(config_path.string() + " already exists (use --force to overwrite)");
        return 1;
    }
    fs::create_directories(config_path.parent_path());
    std::ofstream out(config_path);
    if (!out) {
        ui::error("cannot write " + config_path.string());
        return 1;
    }
    out << defaultConfigText();
    ui::summary("Wrote " + config_path.string());
    return 0;
}

int editConfig(const fs::path& config_path) {
    if (!fs::exists(config_path) && initConfig(config_path, false) != 0) return 1;

    const char* editor = getenv("VISUAL");
    if (!editor || !*editor) editor = getenv("EDITOR");
    std::string ed = (editor && *editor) ? editor : "nano";

    std::string path = config_path.string();
    std::vector<char*> argv = {ed.data(), path.data(), nullptr};
    pid_t pid;
    if (posix_spawnp(&pid, ed.c_str(), nullptr, nullptr, argv.data(), environ) != 0) {
        ui::error("could not launch editor '" + ed + "' (set $EDITOR)");
        return 1;
    }
    int status = 0;
    waitpid(pid, &status, 0);

    // Validate after editing so mistakes surface immediately.
    try {
        loadConfig(config_path);
    } catch (const ConfigError& e) {
        ui::warn(std::string("config has an error: ") + e.what());
        return 1;
    }
    return 0;
}

} // namespace

int configCommand(const Config& config, const fs::path& config_path,
                  const std::string& action, bool force) {
    if (action == "show") return showConfig(config, config_path);
    if (action == "path") {
        std::cout << config_path.string() << "\n";
        return 0;
    }
    if (action == "init") return initConfig(config_path, force);
    if (action == "edit") return editConfig(config_path);
    ui::error("unknown config action '" + action + "' (expected show, path, init or edit)");
    return 2;
}
