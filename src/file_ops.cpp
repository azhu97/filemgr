#include "file_ops.hpp"

namespace fs = std::filesystem;

std::string downloadPath() {
    const char* homeDir = getenv("HOME");
    if (homeDir == nullptr) {
        throw std::runtime_error("Could not determine home directory.");
    } 
    return (std::string(homeDir) + "/Downloads");
}

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
        {".zip", "ARCHIVES"},
        {".rar", "ARCHIVES"},
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

        std::string path_extension = entry.path().extension().string();

        if (typeMap.find(path_extension) != typeMap.end()) {
            std::string targetDir = path + "/" + typeMap[path_extension];
            fs::create_directories(targetDir); // create target directory if it doesn't exist

            // newPath is equal to targetDir + "/" + filename
            fs::path newPath = targetDir + "/" + entry.path().filename().string();
            
            // move entry from old path to new path using rename
            fs::rename(entry.path(), newPath);

            // print statement
            std::cout << "Moved " << entry.path().filename().string()
                      << " -> " << targetDir << "\n";
        }
    }
}

int main(int argc, char const *argv[])
{
    sortByType();
    return 0;
}
