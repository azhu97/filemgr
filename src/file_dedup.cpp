#include "file_dedup.hpp"

namespace fs = std::filesystem;

void deduplicateFiles() {
    std::string download_path = downloadPath();
    fs::path duplicates_path = fs::path(download_path) / "DUPLICATES";

    std::cout << "Deduplicating files in: " << download_path << "\n";

    std::unordered_map<std::string, fs::path> hash_map;

    fs::create_directories(duplicates_path);

    for (const auto& entry : fs::recursive_directory_iterator(download_path)) {
        // Skip the DUPLICATES directory itself
        if (entry.path().string().find(duplicates_path.string()) != std::string::npos) {
            continue;
        }

        // Only process regular files
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string file_hash = computeFileHash(entry.path());
        if (file_hash.empty()) {
            std::cerr << "Failed to hash: " << entry.path() << "\n";
            continue;
        }

        if (hash_map.find(file_hash) == hash_map.end()) {
            hash_map[file_hash] = entry.path();
        } else {
            // Found duplicate
            fs::path old_file = hash_map[file_hash];
            fs::path new_file = entry.path();

            struct stat old_stat, new_stat;
            if (stat(old_file.c_str(), &old_stat) != 0 || 
                stat(new_file.c_str(), &new_stat) != 0) {
                std::cerr << "Failed to stat files\n";
                continue;
            }

            time_t old_time = old_stat.st_birthtimespec.tv_sec;
            time_t new_time = new_stat.st_birthtimespec.tv_sec;

            fs::path file_to_move;
            fs::path file_to_keep;

            if (old_time < new_time) {
                // old_file is older, move new_file
                file_to_move = new_file;
                file_to_keep = old_file;
            } else {
                // new_file is older, move old_file
                file_to_move = old_file;
                file_to_keep = new_file;
                hash_map[file_hash] = new_file;
            }

            fs::path dest = duplicates_path / file_to_move.filename();
            int counter = 1;
            while (fs::exists(dest)) {
                std::string stem = file_to_move.stem().string();
                std::string ext = file_to_move.extension().string();
                dest = duplicates_path / (stem + "_" + std::to_string(counter) + ext);
                counter++;
            }

            try {
                fs::rename(file_to_move, dest);
                std::cout << "Moved duplicate: " << file_to_move.filename() 
                          << " -> DUPLICATES/" << dest.filename() << "\n";
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error moving file: " << e.what() << "\n";
            }
        }
    }
}