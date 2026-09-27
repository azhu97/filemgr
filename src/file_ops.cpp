#include "file_ops.hpp"
#include "config.hpp"

namespace fs = std::filesystem;

int sortByType(const Context& ctx) {
    const fs::path& path = ctx.root;
    ui::info("Sorting files in " + ui::bold(path.string()));

    // Extension -> folder mapping from the config (built-in defaults plus user categories)
    const std::map<std::string, std::string> typeMap = ctx.config->extensionMap();

    std::size_t moved = 0;
    for (const auto& entry : fs::directory_iterator(path)) {
        if (!entry.is_regular_file() || isHidden(entry.path())) {
            continue; // skip directories, symlinks to dirs, dotfiles
        }

        // Match extensions case-insensitively so photo.JPG sorts like photo.jpg
        auto it = typeMap.find(lowerExtension(entry.path()));
        if (it == typeMap.end()) {
            ui::detail("  skip " + entry.path().filename().string() + " (unknown type)");
            continue;
        }

        fs::path newPath = safeMove(ctx, entry.path(), path / it->second);
        if (!newPath.empty()) {
            ui::action("sort", entry.path().filename().string(), displayPath(ctx, newPath));
            moved++;
        }
    }

    ui::summary((ctx.dry_run ? "Would sort " : "Sorted ") + ui::plural(moved, "file"));
    return 0;
}
