# Near-Duplicate Detection

## Problem

`dedup` (`src/file_dedup.cpp`) only catches byte-identical files via SHA-256. It misses the far more common real-world case: the same photo saved twice at different resolutions or JPEG quality, a screenshot re-compressed by Slack/Discord, or a document re-exported from the same source. These have different bytes and different hashes, so they pile up untouched.

## Goal

Extend `dedup` to also flag *near*-duplicate images: visually similar files that aren't byte-identical. Keep exact-hash dedup as-is (fast, zero false positives) and add a second pass for near-dupes as an opt-in, since it's fuzzier and needs a human-tunable threshold.

## Approach

**Perceptual hashing (pHash/dHash)** on images:

1. Decode image, downscale to a small fixed grid (e.g. 32x32 for pHash's DCT step, or 9x8 for dHash).
2. For dHash: convert to grayscale, compare adjacent pixel brightness left-to-right to produce a 64-bit fingerprint (1 bit per comparison). Cheap, no DCT needed — good starting point.
3. For pHash (more robust to scaling/compression artifacts): grayscale → resize → DCT → keep low-frequency coefficients → threshold against the median to get a 64-bit hash.
4. Compare hashes via **Hamming distance**. Distance 0 = same image; small distance (e.g. <= 5 out of 64 bits) = likely near-duplicate; tune empirically.

**Why dHash first**: no DCT library dependency, ~10 lines of bit-twiddling, and it's the same "compute a hash, compare hashes" shape the codebase already has in `computeFileHash()` — an easy mental model to extend rather than replace.

## Scope for v1

- Only images (extensions already known to `file_ops.cpp`'s IMAGES bucket: jpg/jpeg/png/gif/heic). Skip video/audio/documents for now — perceptual hashing for those is a different, harder problem (frame sampling, text extraction).
- Decode with a small header-only or single-dependency image library (e.g. `stb_image.h`) rather than pulling in a heavy image processing framework — keeps the build a single `g++` invocation, consistent with how this project builds today.
- New function `computePerceptualHash(const fs::path&)` in `utils.hpp/cpp`, parallel to the existing `computeFileHash()`.
- Near-dupe pass runs *after* the existing exact-hash pass, only on files that survived it (i.e. weren't exact dupes), and only within the IMAGES-typed subset.
- Reuse the existing `ThreadSafeQueue<T>` + worker pool pattern from `deduplicateFiles()` for hashing in parallel — no new concurrency primitives needed.
- Near-dupes move to `DUPLICATES/NEAR/` (subfolder, so exact and near dupes are distinguishable and near-dupes are easy to review/undo before deleting).

## Open questions

- Threshold tuning: what Hamming distance cutoff balances false positives (different-but-similar photos, e.g. burst shots) against false negatives (misses on heavily re-compressed images)? Needs empirical testing against a real Downloads folder, not just guessing a number.
- Which of two near-duplicates to keep — same "oldest wins" rule as exact dedup, or prefer the higher-resolution/larger file (since a near-dupe pair is often a full-res original + a compressed re-export, where "oldest" and "best quality" don't always coincide)?
- CLI surface: a new flag on `dedup` (e.g. `dedup --near`) vs. a separate `dedup-near` command. Leaning toward a flag since it's the same conceptual operation with a fuzzier match.

## Out of scope for v1

- Non-image near-duplicates (documents, audio, video).
- A GUI/preview step before moving files (the "move to DUPLICATES/NEAR for review" step is the safety net instead).
- Locality-sensitive hashing / bucketing for scaling to huge file counts — a Downloads folder is small enough that O(n²) hash comparisons are fine to start; revisit only if it's actually slow.
