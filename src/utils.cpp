#include "utils.hpp"
#include "util.h"
#include <filesystem>


namespace fs = std::filesystem;

bool isInAllowedLocation(const fs::path& file_path, std::string download_path) {
    // Get the relative path from Downloads
    fs::path relative = fs::relative(file_path, download_path);
    
    // If file is directly in Downloads (no parent directories)
    if (!relative.has_parent_path() || relative.parent_path() == ".") {
        return true;
    }
    
    // Check if first directory in path is a filemgr directory
    std::string first_dir = relative.begin()->string();
    return filemgr_directories.find(first_dir) != filemgr_directories.end();
}