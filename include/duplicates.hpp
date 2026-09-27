#pragma once
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

// Groups byte-identical files. Files are first bucketed by size (a file with a
// unique size can't have a twin, so it is never read), then same-size files
// are SHA-256 hashed in parallel. Empty files are ignored.
// Returns only groups with 2+ members; order within a group is unspecified.
std::vector<std::vector<fs::path>> findExactDuplicates(const std::vector<fs::path>& files,
                                                       unsigned threads);

// Every regular, non-hidden file dedup is allowed to act on: inside the root
// or filemgr's folders, and not already in DUPLICATES/.
std::vector<fs::path> dedupCandidates(const fs::path& root);
