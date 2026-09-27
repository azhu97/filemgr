#pragma once
#include <unordered_set>
#include <string>
#include <filesystem>
#include <CommonCrypto/CommonDigest.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace fs = std::filesystem;

inline std::unordered_set<std::string> filemgr_directories = {
    "IMAGES", "VIDEOS", "AUDIO", "DOCUMENTS", "COMPRESSED", "INSTALLERS", "DUPLICATES", "CODE"
};

bool isInAllowedLocation(const fs::path& file_path, std::string download_path);

std::string computeFileHash(const fs::path& file_path);

std::string downloadPath();

void putFileInDownload(const fs::path& file_path);

// Returns a path inside dest_dir for `filename` that does not exist yet,
// appending _1, _2, ... to the stem on collision.
fs::path uniqueDestination(const fs::path& dest_dir, const fs::path& filename);

// Moves src into dest_dir without ever overwriting an existing file.
// Falls back to copy + remove when src and dest_dir are on different volumes.
// Returns the final path, or an empty path on failure (error printed to stderr).
fs::path safeMove(const fs::path& src, const fs::path& dest_dir);

// Lower-cased copy of a file's extension, e.g. "photo.JPG" -> ".jpg".
std::string lowerExtension(const fs::path& file_path);

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;

public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(value));
        cv.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !queue.empty() || finished; });
        if (queue.empty()) return false;
        value = std::move(queue.front());
        queue.pop();
        return true;
    }

    void set_finished() {
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
        cv.notify_all();
    }
};