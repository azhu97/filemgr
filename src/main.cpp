#include <iostream>
#include <string>
#include "file_ops.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./filemgr <command>\n";
        return 0;
    }

    std::string cmd = argv[1];

    if (cmd == "sort-by-type") {
        sortByType(); // Call function to sort files by type
    } else {
        std::cout << "Unknown command\n";
    }

    return 0;
}