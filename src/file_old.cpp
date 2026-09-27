#include "file_old.hpp"

namespace fs = std::filesystem;

void emptyOldDirectory() {
    std::string download_path = downloadPath();
    fs::path old_path = fs::path(download_path) / "OLD";
    
    if (!fs::exists(old_path) || !fs::is_directory(old_path)) {
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(old_path)) {
        fs::path dest = safeMove(entry.path(), download_path);
        if (!dest.empty()) {
            std::cout << "Moved from OLD: " << entry.path().filename()
                      << " -> " << dest.filename() << "\n";
        }
    }
}

void archiveOld(int x) {
    const long long seconds_threshold = static_cast<long long>(x) * 60 * 60 * 24; // x days to seconds
    
    emptyOldDirectory();
    
    std::string download_path = downloadPath();
    fs::path old_path = fs::path(download_path) / "OLD";
    
    if (!fs::exists(old_path)) {
        fs::create_directory(old_path);
    }
    
    // Get current time using filesystem clock
    auto now = fs::file_time_type::clock::now();
    
    for (const auto& entry : fs::recursive_directory_iterator(download_path)) {
        // Skip non-regular files and hidden files
        if (!entry.is_regular_file() || entry.path().filename().string()[0] == '.') {
            continue;
        }

        if (!isInAllowedLocation(entry.path(), download_path)) {
            continue;
        }


        // Calculate file age
        auto file_time = fs::last_write_time(entry.path());
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - file_time);
        long long age_seconds = age.count();
        
        // Archive if older than threshold
        if (age_seconds >= seconds_threshold) {
            fs::path dest = safeMove(entry.path(), old_path);
            if (!dest.empty()) {
                std::cout << "Archived old file: " << entry.path().filename()
                          << " -> OLD/" << dest.filename() << "\n";
            }
        }
    }
}