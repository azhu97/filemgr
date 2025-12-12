#include "file_recent.hpp"

namespace fs = std::filesystem;

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