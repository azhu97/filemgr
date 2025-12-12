#include "file_old.hpp"

namespace fs = std::filesystem;

void emptyOldDirectory() {
    std::string download_path = downloadPath();
    fs::path old_path = fs::path(download_path) / "OLD";
    
    if (!fs::exists(old_path) || !fs::is_directory(old_path)) {
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(old_path)) {
        fs::path dest = fs::path(download_path) / entry.path().filename();
        
        // Handle filename collisions
        int counter = 1;
        while (fs::exists(dest)) {
            std::string stem = entry.path().stem().string();
            std::string ext = entry.path().extension().string();
            dest = fs::path(download_path) / (stem + "_" + std::to_string(counter) + ext);
            counter++;
        }
        
        try {
            fs::rename(entry.path(), dest);
            std::cout << "Moved from OLD: " << entry.path().filename() 
                      << " -> " << dest.filename() << "\n";
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error moving file from OLD: " << e.what() << "\n";
        }
    }
}

void archiveOld(int x) {
    const long seconds_threshold = x * 60 * 60 * 24; // x days to seconds
    
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
        long age_seconds = age.count();
        
        // Archive if older than threshold
        if (age_seconds >= seconds_threshold) {
            fs::path dest = old_path / entry.path().filename();
            
            // Handle filename collisions
            int counter = 1;
            while (fs::exists(dest)) {
                std::string stem = entry.path().stem().string();
                std::string ext = entry.path().extension().string();
                dest = old_path / (stem + "_" + std::to_string(counter) + ext);
                counter++;
            }
            
            try {
                fs::rename(entry.path(), dest);
                std::cout << "Archived old file: " << entry.path().filename() 
                          << " -> OLD/" << dest.filename() << "\n";
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error archiving file: " << e.what() << "\n";
            }
        }
    }
}