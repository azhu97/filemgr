#include "file_dedup.hpp"
#include <thread>
#include <vector>
#include <mutex>

namespace fs = std::filesystem;

void deduplicateFiles() {
    std::string download_path = downloadPath();
    fs::path duplicates_path = fs::path(download_path) / "DUPLICATES";

    std::cout << "Deduplicating files in: " << download_path << "\n";
    fs::create_directories(duplicates_path);

    // Shared resources
    std::unordered_map<std::string, fs::path> hash_map;
    std::mutex map_mutex; 
    ThreadSafeQueue<fs::path> task_queue;

    // Worker function
    auto worker = [&]() {
        fs::path current_file;
        while (task_queue.pop(current_file)) {
            std::string file_hash = computeFileHash(current_file);
            if (file_hash.empty()) {
                std::cerr << "Failed to hash: " << current_file << "\n";
                continue;
            }

            // Lock the map to check/insert and handle moves safely
            std::lock_guard<std::mutex> lock(map_mutex);
            if (hash_map.find(file_hash) == hash_map.end()) {
                hash_map[file_hash] = current_file;
            } else {
                fs::path existing_file = hash_map[file_hash];
                fs::path new_file = current_file;

                struct stat old_stat, new_stat;
                if (stat(existing_file.c_str(), &old_stat) != 0 || 
                    stat(new_file.c_str(), &new_stat) != 0) {
                    continue;
                }

                // Logic to keep the oldest file
                time_t old_time = old_stat.st_birthtimespec.tv_sec;
                time_t new_time = new_stat.st_birthtimespec.tv_sec;

                fs::path file_to_move = (old_time < new_time) ? new_file : existing_file;
                if (old_time >= new_time) {
                    hash_map[file_hash] = new_file; // Update map if the newer file was actually older
                }

                // Handle filename collisions in DUPLICATES folder
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
    };

    // 1. Start worker threads
    unsigned int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    // 2. Producer: Scan directory and push to queue
    for (const auto& entry : fs::recursive_directory_iterator(download_path)) {
        // Skip the DUPLICATES directory itself and non-regular files
        if (entry.path().string().find(duplicates_path.string()) != std::string::npos || 
            !entry.is_regular_file()) {
            continue;
        }

        if (!isInAllowedLocation(entry.path(), download_path)) {
            continue;
        }

        task_queue.push(entry.path());
    }

    // 3. Signal completion and join threads
    task_queue.set_finished();
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}