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