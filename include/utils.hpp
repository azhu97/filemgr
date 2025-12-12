#pragma once
#include <unordered_set>
#include <string>
#include <filesystem>
#include <CommonCrypto/CommonDigest.h>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

inline std::unordered_set<std::string> filemgr_directories = {
    "IMAGES", "VIDEOS", "AUDIO", "DOCUMENTS", "COMPRESSED", "INSTALLERS", "DUPLICATES", "CODE"
};

bool isInAllowedLocation(const fs::path& file_path, std::string download_path);

std::string computeFileHash(const fs::path& file_path);

std::string downloadPath();

void putFileInDownload(const fs::path& file_path);