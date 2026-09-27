#pragma once
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// User configuration, loaded from an INI-style file:
//
//   [general]
//   root = ~/Downloads
//   remote = gdrive
//
//   [categories]
//   SCREENSHOTS = png heic      # later entries win for a given extension
//
// See defaultConfigText() for every supported key.

struct ConfigError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Category {
    std::string folder;               // e.g. "IMAGES"
    std::vector<std::string> extensions;  // lower-case, with leading dot
};

struct Config {
    fs::path source;          // File this was loaded from; empty for built-in defaults
    fs::path root;            // Empty means "use ~/Downloads"
    std::string remote = "gdrive";
    int old_days = 30;
    int recent_count = 5;
    unsigned threads = 0;     // 0 = one per CPU core
    std::vector<Category> categories;

    // Extension (".jpg") -> folder ("IMAGES"). Later categories win.
    std::map<std::string, std::string> extensionMap() const;
    // Every folder filemgr treats as its own (category folders + DUPLICATES).
    std::unordered_set<std::string> managedFolders() const;
};

// Built-in defaults (used when no config file exists).
Config defaultConfig();

// Loads `file` on top of the defaults. A missing file yields the defaults.
// Throws ConfigError ("config:12: unknown key 'foo'") on malformed input.
Config loadConfig(const fs::path& file);

// $FILEMGR_CONFIG, else $XDG_CONFIG_HOME/filemgr/config, else ~/.config/filemgr/config.
fs::path defaultConfigPath();

// Commented starter config written by `filemgr config init`.
std::string defaultConfigText();

// Expands a leading "~/" to $HOME.
fs::path expandHome(const std::string& path);
