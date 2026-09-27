#pragma once
#include <filesystem>

namespace fs = std::filesystem;

// Run-wide settings shared by every command.
struct Context {
    fs::path root;         // Folder being managed (defaults to ~/Downloads)
    bool dry_run = false;  // Report what would happen without touching anything
};
