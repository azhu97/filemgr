#include <iostream>
#include <string>
#include "file_ops.hpp"
#include "file_recent.hpp"
#include "file_dedup.hpp"
#include "file_old.hpp"
#include "file_upload.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: filemgr <command> [options]\n";
        std::cout << "Commands:\n";
        std::cout << "  sort             Sort files into directories\n";
        std::cout << "  recent <n>       Process n most recent files, or 5 by default\n";
        std::cout << "  dedup            Remove duplicate files\n";
        std::cout << "  old <n>         Archive files older than n days\n";
        std::cout << "  upload <folder>  Upload folder from Downloads to Google Drive\n";
        return 1;
    }

    std::string command = argv[1];
    if (command == "sort") {
        sortByType();
    } else if (command == "recent") {
        int n = 5; // By Default show 5 recent files
        if (argc >= 3) {
            n = std::stoi(argv[2]);
        }
        recentFile(n);
    } else if (command == "dedup") {
        deduplicateFiles();
    } else if (command == "old") {
        int n = 30; // Default to 30 days
        if (argc >= 3) {
            n = std::stoi(argv[2]);
        }
        archiveOld(n);
    } else if (command == "upload") {
        if (argc < 3) {
            std::cout << "Usage: filemgr upload <folder>\n";
            return 1;
        }
        std::string folder_name = argv[2];
        uploadFolder(folder_name);
    } else {
        std::cout << "Unknown command: " << command << "\n";
        std::cout << "Usage: filemgr <command> [options]\n";
        std::cout << "Commands:\n";
        std::cout << "  sort             Sort files into directories\n";
        std::cout << "  recent <n>       Process n most recent files, or 5 by default\n";
        std::cout << "  dedup            Remove duplicate files\n";
        std::cout << "  old <n>         Archive files older than n days\n";
        std::cout << "  upload <folder>  Upload folder from Downloads to Google Drive\n";
        return 1;
    }
}