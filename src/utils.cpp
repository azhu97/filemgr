#include "utils.hpp"
#include <algorithm>
#include <cctype>


namespace fs = std::filesystem;

bool isInAllowedLocation(const fs::path& file_path, const fs::path& download_path) {
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

fs::path downloadPath() {
    if (const char* override_root = getenv("FILEMGR_ROOT"); override_root && *override_root) {
        return fs::path(override_root);
    }
    const char* homeDir = getenv("HOME");
    if (homeDir == nullptr) {
        throw std::runtime_error("Could not determine home directory.");
    }
    return fs::path(homeDir) / "Downloads";
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

fs::path safeMove(const Context& ctx, const fs::path& src, const fs::path& dest_dir) {
    std::error_code ec;
    if (ctx.dry_run) {
        return uniqueDestination(dest_dir, src.filename());
    }

    fs::create_directories(dest_dir, ec);
    if (ec) {
        ui::error("cannot create " + dest_dir.string() + ": " + ec.message());
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
        ui::error("cannot move " + src.string() + ": " + ec.message());
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

std::string displayPath(const Context& ctx, const fs::path& p) {
    std::error_code ec;
    fs::path rel = fs::relative(p, ctx.root, ec);
    if (ec || rel.empty() || *rel.begin() == "..") return p.string();
    return rel.string();
}

bool isHidden(const fs::path& p) {
    std::string name = p.filename().string();
    return !name.empty() && name[0] == '.';
}

std::string topLevelFolder(const fs::path& p, const fs::path& root) {
    fs::path rel = p.lexically_relative(root);
    if (rel.empty() || !rel.has_parent_path()) return "";
    return rel.begin()->string();
}
