#pragma once
#include "utils.hpp"

// Default maximum Hamming distance (out of 64 bits) for two images to count
// as near-duplicates. Tuned so re-encoded/resized copies match while burst
// shots of a moving subject usually do not.
constexpr int kDefaultNearThreshold = 6;

// Maximum mean per-channel color difference (0-255) between near-duplicates.
// Re-encoding shifts colors by a few units; recoloring shifts them by far more.
constexpr int kMaxColorDistance = 16;

// Finds visually similar images (resized, re-compressed, format-converted
// copies) and moves all but the best copy of each group into
// DUPLICATES/NEAR/. The best copy is the one with the most pixels, then the
// largest file, then the oldest.
int findNearDuplicates(const Context& ctx, int threshold);
