#pragma once
#include <filesystem>

class Journal;

namespace fs = std::filesystem;

// Run-wide settings shared by every command.
struct Context {
    fs::path root;         // Folder being managed (defaults to ~/Downloads)
    bool dry_run = false;  // Report what would happen without touching anything
    Journal* journal = nullptr;  // Records every move for undo; null disables it
};
