#include "file_dedup.hpp"
#include <thread>
#include <vector>
#include <mutex>
#include <map>

namespace fs = std::filesystem;

namespace {
// Birth time (creation) on macOS; falls back to 0 if stat fails.
time_t birthTime(const fs::path& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return 0;
    return st.st_birthtimespec.tv_sec;
}
} // namespace

int deduplicateFiles(const Context& ctx) {
    const fs::path& download_path = ctx.root;
    fs::path duplicates_path = download_path / "DUPLICATES";

    ui::info("Deduplicating files in " + ui::bold(download_path.string()));

    // 1. Scan: group candidate files by size. Files with a unique size cannot
    //    have a byte-identical twin, so they are never hashed.
    std::map<std::uintmax_t, std::vector<fs::path>> by_size;
    std::size_t scanned = 0;
    for (const auto& entry : fs::recursive_directory_iterator(
             download_path, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file() || isHidden(entry.path())) {
            continue;
        }
        // Skip the DUPLICATES directory itself and anything outside filemgr's folders
        const fs::path& p = entry.path();
        if (!isInAllowedLocation(p, download_path) || topLevelFolder(p, download_path) == "DUPLICATES") {
            continue;
        }
        std::error_code ec;
        auto size = entry.file_size(ec);
        if (ec || size == 0) continue; // empty files are trivially "identical"; leave them
        by_size[size].push_back(p);
        scanned++;
    }

    // Shared resources
    std::unordered_map<std::string, fs::path> hash_map;
    std::mutex map_mutex;
    ThreadSafeQueue<fs::path> task_queue;
    std::size_t moved = 0;
    std::uintmax_t reclaimed = 0;

    // Worker function
    auto worker = [&]() {
        fs::path current_file;
        while (task_queue.pop(current_file)) {
            std::string file_hash = computeFileHash(current_file);
            if (file_hash.empty()) {
                ui::warn("failed to hash " + current_file.string());
                continue;
            }

            // Lock the map to check/insert and handle moves safely
            std::lock_guard<std::mutex> lock(map_mutex);
            auto it = hash_map.find(file_hash);
            if (it == hash_map.end()) {
                hash_map[file_hash] = current_file;
                continue;
            }

            // Keep whichever copy was created first; move the other.
            fs::path existing_file = it->second;
            fs::path file_to_move = current_file;
            if (birthTime(current_file) < birthTime(existing_file)) {
                file_to_move = existing_file;
                it->second = current_file;
            }

            std::error_code ec;
            auto size = fs::file_size(file_to_move, ec);
            fs::path dest = safeMove(ctx, file_to_move, duplicates_path);
            if (!dest.empty()) {
                ui::action("dup", displayPath(ctx, file_to_move),
                           displayPath(ctx, dest) + ui::dim("  (copy of " + displayPath(ctx, it->second) + ")"));
                moved++;
                if (!ec) reclaimed += size;
            }
        }
    };

    // 2. Start worker threads
    unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    // 3. Producer: queue only files that share a size with another file
    std::size_t hashed = 0;
    for (const auto& [size, paths] : by_size) {
        if (paths.size() < 2) continue;
        for (const auto& p : paths) {
            task_queue.push(p);
            hashed++;
        }
    }

    // 4. Signal completion and join threads
    task_queue.set_finished();
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    ui::detail("scanned " + ui::plural(scanned, "file") + ", hashed " + std::to_string(hashed)
               + " using " + ui::plural(num_threads, "thread"));
    ui::summary((ctx.dry_run ? "Would move " : "Moved ") + ui::plural(moved, "duplicate")
                + " (" + ui::humanSize(reclaimed) + ")");
    return 0;
}
