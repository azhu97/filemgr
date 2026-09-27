#pragma once
#include "utils.hpp"
#include <ctime>
#include <string>
#include <vector>

// Read-only analysis of the managed folder, shared by the terminal and HTML
// views of `filemgr stats`.

struct StatBucket {
    std::string name;
    std::size_t count = 0;
    std::uintmax_t bytes = 0;
};

struct FileEntry {
    fs::path path;
    std::uintmax_t bytes = 0;
    std::time_t modified = 0;
};

struct DuplicateGroup {
    std::vector<fs::path> paths;   // First entry is the copy dedup would keep
    std::uintmax_t file_bytes = 0; // Size of one copy
    std::uintmax_t wasted() const { return file_bytes * (paths.size() - 1); }
};

struct FolderStats {
    fs::path root;
    std::time_t generated = 0;
    std::size_t total_files = 0;
    std::uintmax_t total_bytes = 0;

    std::vector<StatBucket> by_category;  // IMAGES, VIDEOS, ..., "Other" (largest first)
    std::vector<StatBucket> by_location;  // "(top level)", IMAGES/, PROTECTED/, ... (largest first)
    std::vector<StatBucket> by_age;       // Fixed order: newest to oldest
    std::vector<FileEntry> largest;       // Top N by size

    bool duplicates_scanned = false;
    std::vector<DuplicateGroup> duplicates;  // Largest waste first
    std::size_t duplicate_files = 0;         // Extra copies (excludes keepers)
    std::uintmax_t duplicate_bytes = 0;

    int stale_days = 30;
    std::size_t stale_files = 0;             // What `filemgr old <stale_days>` would archive
    std::uintmax_t stale_bytes = 0;

    std::size_t unsorted_files = 0;          // Top-level files `sort` would move
};

FolderStats collectStats(const Context& ctx, std::size_t top_n, bool scan_duplicates);

// `filemgr stats`: prints the report, or writes it as HTML when html_path is set.
int showStats(const Context& ctx, std::size_t top_n, bool scan_duplicates, const std::string& html_path);

// Standalone HTML rendering (src/report_html.cpp).
std::string renderHtmlReport(const FolderStats& stats);
