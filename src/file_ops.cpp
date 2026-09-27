#include "file_ops.hpp"

namespace fs = std::filesystem;

int sortByType(const Context& ctx) {
    const fs::path& path = ctx.root;
    ui::info("Sorting files in " + ui::bold(path.string()));

    // mapping for file extensions to directories
    std::map<std::string, std::string> typeMap = {
        {".jpeg", "IMAGES"},
        {".jpg", "IMAGES"},
        {".png", "IMAGES"},
        {".gif", "IMAGES"},
        {".heic", "IMAGES"},
        {".mpg", "VIDEOS"},
        {".mp4", "VIDEOS"},
        {".mkv", "VIDEOS"},
        {".mp3", "AUDIO"},
        {".pdf", "DOCUMENTS"},
        {".docx", "DOCUMENTS"},
        {".txt", "DOCUMENTS"},
        {".pptx", "DOCUMENTS"},
        {".zip", "COMPRESSED"},
        {".tar", "COMPRESSED"},
        {".gz", "COMPRESSED"},
        {".rar", "COMPRESSED"},
        {".exe", "INSTALLERS"},
        {".dmg", "INSTALLERS"},
        {".sql", "CODE"},
        {".cpp", "CODE"},
        {".py", "CODE"},
        {".js", "CODE"},
        {".html", "CODE"},
        {".css", "CODE"},
        {".java", "CODE"},
        {".c", "CODE"},
        {".h", "CODE"},
        {".rb", "CODE"},
        {".go", "CODE"},
        {".rs", "CODE"},
        {".ts", "CODE"}
    };

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
