#include <iostream>
#include <string>
#include "file_ops.hpp"
#include "file_recent.hpp"
#include "file_dedup.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: filemgr <command> [options]\n";
        std::cout << "Commands:\n";
        std::cout << "  sort             Sort files into directories\n";
        std::cout << "  recent <n>       Process n most recent files, or 5 by default\n";
        std::cout << "  dedup            Remove duplicate files\n";
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
    } else {
        std::cout << "Unknown command: " << command << "\n";
        std::cout << "Usage: filemgr <command> [options]\n";
        std::cout << "Commands:\n";
        std::cout << "  sort             Sort files into directories\n";
        std::cout << "  recent <n>       Process n most recent files, or 5 by default\n";
        std::cout << "  dedup            Remove duplicate files\n";
        return 1;
    }
}