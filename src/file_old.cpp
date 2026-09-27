#include "file_old.hpp"

namespace fs = std::filesystem;

namespace {
// Moves everything in OLD/ back to the top level so each run re-evaluates
// file ages from scratch. Returns the number of files restored.
std::size_t emptyOldDirectory(const Context& ctx) {
    fs::path old_path = ctx.root / "OLD";
    if (!fs::is_directory(old_path)) {
        return 0;
    }

    std::size_t restored = 0;
    for (const auto& entry : fs::directory_iterator(old_path)) {
        fs::path dest = safeMove(ctx, entry.path(), ctx.root);
        if (!dest.empty()) {
            ui::detail("  restore OLD/" + entry.path().filename().string() + " -> " + dest.filename().string());
            restored++;
        }
    }
    return restored;
}
} // namespace

int archiveOld(const Context& ctx, int days) {
    const long long seconds_threshold = static_cast<long long>(days) * 60 * 60 * 24;
    const fs::path& download_path = ctx.root;
    fs::path old_path = download_path / "OLD";

    ui::info("Archiving files untouched for " + ui::plural(days, "day") + " in " + ui::bold(download_path.string()));

    std::size_t restored = emptyOldDirectory(ctx);

    // Get current time using filesystem clock
    auto now = fs::file_time_type::clock::now();

    // Collect first, then move: moving while iterating recursively is unsafe.
    std::vector<fs::path> stale;
    for (const auto& entry : fs::recursive_directory_iterator(
             download_path, fs::directory_options::skip_permission_denied)) {
        // Skip non-regular files and hidden files
        if (!entry.is_regular_file() || isHidden(entry.path())) {
            continue;
        }
        if (!isInAllowedLocation(entry.path(), download_path)) {
            continue;
        }

        // Calculate file age
        std::error_code ec;
        auto file_time = fs::last_write_time(entry.path(), ec);
        if (ec) continue;
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - file_time);
        if (age.count() >= seconds_threshold) {
            stale.push_back(entry.path());
        }
    }

    std::size_t archived = 0;
    for (const auto& p : stale) {
        fs::path dest = safeMove(ctx, p, old_path);
        if (!dest.empty()) {
            ui::action("old", displayPath(ctx, p), displayPath(ctx, dest));
            archived++;
        }
    }

    if (restored > 0) {
        ui::detail("restored " + ui::plural(restored, "file") + " from OLD/ before re-archiving");
    }
    ui::summary((ctx.dry_run ? "Would archive " : "Archived ") + ui::plural(archived, "file"));
    return 0;
}
