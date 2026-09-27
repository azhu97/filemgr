#pragma once
#include "filter.hpp"
#include "utils.hpp"

struct FindOptions {
    FileFilter filter;
    fs::path within;             // Search only this subfolder of the root (empty = whole root)
    std::string sort = "date";   // date (newest first), size (largest first), name
    std::size_t limit = 0;       // 0 = no limit
    bool paths_only = false;     // Print bare absolute paths (for piping)
    bool null_separated = false; // With paths_only: separate with NUL (xargs -0)
    bool include_hidden = false;
};

// `filemgr find`: read-only search across the managed folder (including user
// folders, since nothing is modified).
int findFiles(const Context& ctx, const FindOptions& options);
