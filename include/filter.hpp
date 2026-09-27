#pragma once
#include <ctime>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// File predicates shared by `find` and the rules engine.

struct FilterError : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

// "500", "10K", "1.5M", "2G", "1T" -> bytes (binary units, case-insensitive,
// optional trailing "B": "10MB").
std::uintmax_t parseSize(const std::string& text);

// "12h", "7d", "2w", "3m" (30-day months), "1y" -> seconds. A bare number means days.
long long parseAge(const std::string& text);

struct FileFilter {
    std::vector<std::string> names;       // Case-insensitive globs on the filename; any may match.
                                          // A pattern without * ? [ matches as a substring.
    std::set<std::string> extensions;     // Lower-case with dot (".jpg"); empty = any
    std::string category;                 // Folder name from the config ("IMAGES") or "Other"
    std::optional<std::uintmax_t> min_size, max_size;
    std::optional<long long> min_age, max_age;  // Seconds since last modification

    // Adds comma/space separated extensions ("jpg, .PNG").
    void addExtensions(const std::string& list);

    bool empty() const;
    bool matches(const fs::path& path, std::uintmax_t size, std::time_t modified,
                 const std::map<std::string, std::string>& typeMap, std::time_t now) const;
};

// Last modification time as time_t (for comparing with std::time(nullptr)).
std::time_t modifiedTime(const fs::path& p);
