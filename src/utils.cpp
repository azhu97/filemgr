#include "utils.hpp"
#include <algorithm>
#include <cctype>


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
    fs::path download_path = downloadPath();
    if (file_path.parent_path() == download_path) {
        std::cout << "File already in Downloads: " << file_path.filename() << "\n";
        return;
    }

    fs::path dest = safeMove(file_path, download_path);
    if (!dest.empty()) {
        std::cout << "Moved recent file: " << file_path.filename()
                  << " -> " << dest.filename() << "\n";
    }
}

fs::path uniqueDestination(const fs::path& dest_dir, const fs::path& filename) {
    fs::path dest = dest_dir / filename;
    std::string stem = filename.stem().string();
    std::string ext = filename.extension().string();
    int counter = 1;
    while (fs::exists(fs::symlink_status(dest))) {
        dest = dest_dir / (stem + "_" + std::to_string(counter) + ext);
        counter++;
    }
    return dest;
}

fs::path safeMove(const fs::path& src, const fs::path& dest_dir) {
    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    if (ec) {
        std::cerr << "Error creating " << dest_dir << ": " << ec.message() << "\n";
        return {};
    }

    fs::path dest = uniqueDestination(dest_dir, src.filename());
    fs::rename(src, dest, ec);
    if (ec == std::errc::cross_device_link) {
        // Different volume: copy, then remove the original only if the copy succeeded.
        ec.clear();
        fs::copy(src, dest, fs::copy_options::recursive, ec);
        if (!ec) {
            fs::remove_all(src, ec);
        }
    }
    if (ec) {
        std::cerr << "Error moving " << src << ": " << ec.message() << "\n";
        return {};
    }
    return dest;
}

std::string lowerExtension(const fs::path& file_path) {
    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext;
}
