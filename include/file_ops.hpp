#pragma once
#include "utils.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

int sortByType(const Context& ctx);

// Moves a single top-level file into its type folder. Returns the new path,
// or an empty path if the file was skipped (unknown type, hidden, not a
// regular file) or could not be moved. Shared by sort and watch.
fs::path sortOneFile(const Context& ctx, const fs::path& file,
                     const std::map<std::string, std::string>& typeMap);
