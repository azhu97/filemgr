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
#include "context.hpp"
#include "ui.hpp"

namespace fs = std::filesystem;

inline std::unordered_set<std::string> filemgr_directories = {
    "IMAGES", "VIDEOS", "AUDIO", "DOCUMENTS", "COMPRESSED", "INSTALLERS", "DUPLICATES", "CODE"
};

// True when file_path sits directly in root or inside one of filemgr's own
// folders. Anything under a user-created folder (e.g. PROTECTED) is off-limits.
bool isInAllowedLocation(const fs::path& file_path, const fs::path& root);

std::string computeFileHash(const fs::path& file_path);

// Default managed folder: $FILEMGR_ROOT if set, otherwise ~/Downloads.
fs::path downloadPath();

// Returns a path inside dest_dir for `filename` that does not exist yet,
// appending _1, _2, ... to the stem on collision.
fs::path uniqueDestination(const fs::path& dest_dir, const fs::path& filename);

// Moves src into dest_dir without ever overwriting an existing file.
// Falls back to copy + remove when src and dest_dir are on different volumes.
// In dry-run mode nothing is touched and the would-be destination is returned.
// Returns the final path, or an empty path on failure (error already reported).
fs::path safeMove(const Context& ctx, const fs::path& src, const fs::path& dest_dir);

// Like safeMove, but moves src to `desired` (a full path), choosing a unique
// variant of desired's filename if it is taken. Used by undo to restore
// original names.
fs::path safeMoveTo(const Context& ctx, const fs::path& src, const fs::path& desired);

// Lower-cased copy of a file's extension, e.g. "photo.JPG" -> ".jpg".
std::string lowerExtension(const fs::path& file_path);

// Path relative to the managed root, for display ("IMAGES/a.png").
std::string displayPath(const Context& ctx, const fs::path& p);

// First path component of p relative to root ("IMAGES" for root/IMAGES/a.png),
// or "" when p sits directly in root.
std::string topLevelFolder(const fs::path& p, const fs::path& root);

// True for dotfiles such as .DS_Store.
bool isHidden(const fs::path& p);

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