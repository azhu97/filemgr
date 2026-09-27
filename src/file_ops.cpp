#include "file_ops.hpp"

namespace fs = std::filesystem;

void sortByType() {
    std::string path = downloadPath();
    std::cout << "Sorting files in: " << path << "\n";

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

    for (const auto& entry : fs::directory_iterator(path)) {
        if (!entry.is_regular_file()) {
            continue; // skip non-regular files
        }

        // Match extensions case-insensitively so photo.JPG sorts like photo.jpg
        auto it = typeMap.find(lowerExtension(entry.path()));
        if (it == typeMap.end()) {
            continue;
        }

        fs::path targetDir = fs::path(path) / it->second;
        fs::path newPath = safeMove(entry.path(), targetDir);
        if (!newPath.empty()) {
            std::cout << "Moved " << entry.path().filename().string()
                      << " -> " << it->second << "/" << newPath.filename().string() << "\n";
        }
    }
}
