#include "file_dedup.hpp"
#include "config.hpp"
#include "duplicates.hpp"
#include <algorithm>

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
    const fs::path duplicates_path = ctx.root / "DUPLICATES";
    ui::info("Deduplicating files in " + ui::bold(ctx.root.string()));

    auto candidates = dedupCandidates(ctx.root);
    auto groups = findExactDuplicates(candidates, ctx.config->threads);

    std::size_t moved = 0;
    std::uintmax_t reclaimed = 0;
    for (auto& group : groups) {
        // Keep whichever copy was created first (path breaks ties); move the rest.
        std::sort(group.begin(), group.end(), [](const fs::path& a, const fs::path& b) {
            time_t ta = birthTime(a), tb = birthTime(b);
            return ta != tb ? ta < tb : a < b;
        });
        const fs::path& keeper = group.front();
        for (std::size_t i = 1; i < group.size(); ++i) {
            std::error_code ec;
            auto size = fs::file_size(group[i], ec);
            fs::path dest = safeMove(ctx, group[i], duplicates_path);
            if (dest.empty()) continue;
            ui::action("dup", displayPath(ctx, group[i]),
                       displayPath(ctx, dest) + ui::dim("  (copy of " + displayPath(ctx, keeper) + ")"));
            moved++;
            if (!ec) reclaimed += size;
        }
    }

    ui::detail("scanned " + ui::plural(candidates.size(), "file") + ", found " +
               ui::plural(groups.size(), "duplicate group"));
    ui::summary((ctx.dry_run ? "Would move " : "Moved ") + ui::plural(moved, "duplicate")
                + " (" + ui::humanSize(reclaimed) + ")");
    return 0;
}
