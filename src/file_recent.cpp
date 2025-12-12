#include "file_recent.hpp"


namespace fs = std::filesystem;

std::unordered_set<std::string> filemgr_directories = {
    "IMAGES", "VIDEOS", "AUDIO", "DOCUMENTS", "ARCHIVES", "INSTALLERS", "DUPLICATES", "CODE"
};

// Check if a file is in Downloads root or in a filemgr directory
bool isInAllowedLocation(const fs::path& file_path, const fs::path& download_path) {
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

void putFileInDownload(const fs::path& file_path) {
    std::string download_path = downloadPath();
    fs::path dest = fs::path(download_path) / file_path.filename();
    
    // Handle filename collisions
    int counter = 1;
    while (fs::exists(dest)) {
        std::string stem = file_path.stem().string();
        std::string ext = file_path.extension().string();
        dest = fs::path(download_path) / (stem + "_" + std::to_string(counter) + ext);
        counter++;
    }
    
    try {
        fs::rename(file_path, dest);
        std::cout << "Moved recent file: " << file_path.filename() 
                  << " -> " << dest.filename() << "\n";
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error moving file: " << e.what() << "\n";
    }
}

void recentFile(int x) {
    std::string download_path = downloadPath();
    fs::path download_path_obj(download_path);
    
    using FileTimePair = std::pair<std::time_t, fs::path>;
    std::priority_queue<FileTimePair, std::vector<FileTimePair>, std::less<FileTimePair>> min_heap;
    
    for (const auto& entry: fs::recursive_directory_iterator(download_path)) {
        if (!entry.is_regular_file() || entry.path().filename().string()[0] == '.') {
            continue;
        }
        
        // Skip files not in allowed locations
        if (!isInAllowedLocation(entry.path(), download_path_obj)) {
            continue;
        }
        
        time_t mod_time = fs::last_write_time(entry).time_since_epoch().count();
        min_heap.emplace(mod_time, entry.path());
    }
    
    for (int i = 0; i < x && !min_heap.empty(); ++i) {
        auto [mod_time, file_path] = min_heap.top();
        min_heap.pop();
        putFileInDownload(file_path);
    }
}