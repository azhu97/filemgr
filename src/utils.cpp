#include "utils.hpp"


namespace fs = std::filesystem;

bool isInAllowedLocation(const fs::path& file_path, std::string download_path) {
    // Get the relative path from Downloads
    fs::path relative = fs::relative(file_path, download_path);
    
    // If file is directly in Downloads (no parent directories)
    if (!relative.has_parent_path() || relative.parent_path() == ".") {
        return true;
    }
    
    // Check if first directory in path is a filemgr directory
    std::string first_dir = relative.begin()->string();
    return filemgr_directories.find(first_dir) != filemgr_directories.end();
}

std::string computeFileHash(const fs::path& file_path) {
    std::ifstream file(file_path, std::ifstream::binary);
    if (!file) {
        return "";
    }

    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);

    const size_t buffer_size = 32768;
    char buffer[buffer_size];

    while (file.good()) {
        file.read(buffer, buffer_size);
        CC_SHA256_Update(&ctx, buffer, file.gcount());
    }

    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(hash, &ctx);

    std::string hash_str;
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hash_str += buf;
    }

    return hash_str;
}

std::string downloadPath() {
    const char* homeDir = getenv("HOME");
    if (homeDir == nullptr) {
        throw std::runtime_error("Could not determine home directory.");
    } 
    return (std::string(homeDir) + "/Downloads");
}

void putFileInDownload(const fs::path& file_path) {
    // check if file is already in download path


    std::string download_path = downloadPath();
    fs::path dest = fs::path(download_path) / file_path.filename();

    if (file_path == dest) {
        std::cout << "File already in Downloads: " << file_path.filename() << "\n";
        return;
    }

    // Handle filename collisions
    int counter = 1;
    while (fs::exists(dest)) {
        std::string stem = file_path.stem().string();
        std::string ext = file_path.extension().string();
        dest = fs::path(download_path) / (stem + "_" + std::to_string(counter) + ext);
        counter++;
    }
    
    try {
        fs::rename(file_path, dest);
        std::cout << "Moved recent file: " << file_path.filename() 
                  << " -> " << dest.filename() << "\n";
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error moving file: " << e.what() << "\n";
    }
}

// Create a threadsafe queue
