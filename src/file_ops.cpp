#include "file_ops.hpp"
#include "config.hpp"

namespace fs = std::filesystem;

fs::path sortOneFile(const Context& ctx, const fs::path& file,
                     const std::map<std::string, std::string>& typeMap, bool report) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec) || isHidden(file)) {
        return {}; // skip directories, symlinks to dirs, dotfiles
    }

    // Match extensions case-insensitively so photo.JPG sorts like photo.jpg
    auto it = typeMap.find(lowerExtension(file));
    if (it == typeMap.end()) {
        ui::detail("  skip " + file.filename().string() + " (unknown type)");
        return {};
    }

    fs::path newPath = safeMove(ctx, file, ctx.root / it->second);
    if (!newPath.empty() && report) {
        ui::action("sort", file.filename().string(), displayPath(ctx, newPath));
    }
    return newPath;
}

int sortByType(const Context& ctx) {
    ui::info("Sorting files in " + ui::bold(ctx.root.string()));

    // Extension -> folder mapping from the config (built-in defaults plus user categories)
    const std::map<std::string, std::string> typeMap = ctx.config->extensionMap();

    // Collect first so moving files never disturbs the directory iteration.
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(ctx.root)) {
        files.push_back(entry.path());
    }

    std::size_t moved = 0;
    for (const auto& file : files) {
        if (!sortOneFile(ctx, file, typeMap).empty()) moved++;
    }

    ui::summary((ctx.dry_run ? "Would sort " : "Sorted ") + ui::plural(moved, "file"));
    return 0;
}
