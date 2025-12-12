#pragma once
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <ctime>
#include <CommonCrypto/CommonDigest.h>
#include "file_ops.hpp"

namespace fs = std::filesystem;

std::string computeFileHash(const fs::path& file_path);
void deduplicateFiles();

