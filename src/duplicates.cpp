#include "duplicates.hpp"
#include "utils.hpp"
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

std::vector<fs::path> dedupCandidates(const fs::path& root) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        const fs::path& p = entry.path();
        if (!entry.is_regular_file() || isHidden(p)) continue;
        // Skip anything outside filemgr's folders and the DUPLICATES directory itself
        if (!isInAllowedLocation(p, root) || topLevelFolder(p, root) == "DUPLICATES") continue;
        files.push_back(p);
    }
    return files;
}

std::vector<std::vector<fs::path>> findExactDuplicates(const std::vector<fs::path>& files, unsigned threads) {
    // 1. Bucket by size.
    std::map<std::uintmax_t, std::vector<fs::path>> by_size;
    for (const auto& p : files) {
        std::error_code ec;
        auto size = fs::file_size(p, ec);
        if (ec || size == 0) continue; // empty files are trivially "identical"; leave them
        by_size[size].push_back(p);
    }

    // 2. Hash same-size files with a worker pool.
    std::unordered_map<std::string, std::vector<fs::path>> by_hash;
    std::mutex map_mutex;
    ThreadSafeQueue<fs::path> task_queue;

    if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < threads; ++i) {
        workers.emplace_back([&] {
            fs::path current;
            while (task_queue.pop(current)) {
                std::string hash = computeFileHash(current);
                if (hash.empty()) {
                    ui::warn("failed to hash " + current.string());
                    continue;
                }
                std::lock_guard<std::mutex> lock(map_mutex);
                by_hash[hash].push_back(current);
            }
        });
    }
    for (const auto& [size, paths] : by_size) {
        if (paths.size() < 2) continue;
        for (const auto& p : paths) task_queue.push(p);
    }
    task_queue.set_finished();
    for (auto& t : workers) t.join();

    std::vector<std::vector<fs::path>> groups;
    for (auto& [hash, paths] : by_hash) {
        if (paths.size() > 1) groups.push_back(std::move(paths));
    }
    return groups;
}
