#include "file_recent.hpp"
#include <algorithm>

namespace fs = std::filesystem;

int recentFile(const Context& ctx, int count) {
    const fs::path& download_path = ctx.root;

    // Collect (mtime, path) for every eligible file, then keep the newest `count`.
    using FileTimePair = std::pair<fs::file_time_type, fs::path>;
    std::vector<FileTimePair> files;

    for (const auto& entry : fs::recursive_directory_iterator(
             download_path, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file() || isHidden(entry.path())) {
            continue;
        }

        // Skip files outside filemgr's folders, and duplicates (bringing those
        // back would undo dedup)
        if (!isInAllowedLocation(entry.path(), download_path) ||
            topLevelFolder(entry.path(), download_path) == "DUPLICATES") {
            continue;
        }

        std::error_code ec;
        auto mod_time = fs::last_write_time(entry.path(), ec);
        if (!ec) files.emplace_back(mod_time, entry.path());
    }

    std::size_t n = std::min<std::size_t>(count, files.size());
    std::partial_sort(files.begin(), files.begin() + n, files.end(),
                      [](const FileTimePair& a, const FileTimePair& b) { return a.first > b.first; });

    std::size_t moved = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const fs::path& file_path = files[i].second;
        if (file_path.parent_path() == download_path) {
            ui::action("keep", file_path.filename().string(), "(already at top level)");
            continue;
        }
        fs::path dest = safeMove(ctx, file_path, download_path);
        if (!dest.empty()) {
            ui::action("recent", displayPath(ctx, file_path), dest.filename().string());
            moved++;
        }
    }

    ui::summary((ctx.dry_run ? "Would bring back " : "Brought back ") + ui::plural(moved, "recent file"));
    return 0;
}
