#pragma once
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

// Perceptual hashing for near-duplicate image detection (see
// docs/near-duplicate-detection.md). Images are decoded with macOS ImageIO,
// so JPEG, PNG, GIF, HEIC, WebP, TIFF, BMP and AVIF are all supported.

struct ImageFingerprint {
    bool ok = false;        // false if the file could not be decoded
    std::uint64_t hash = 0; // 64-bit dHash
    int width = 0;          // Original pixel dimensions
    int height = 0;
};

// Computes a 64-bit difference hash (dHash): the image is converted to
// grayscale, shrunk to 9x8, and each bit records whether a pixel is brighter
// than its right-hand neighbour. Visually similar images differ in few bits.
ImageFingerprint computePerceptualHash(const fs::path& file_path);

// Number of differing bits between two hashes (0 = visually identical).
int hammingDistance(std::uint64_t a, std::uint64_t b);

// True for extensions ImageIO can decode.
bool isImageFile(const fs::path& file_path);
