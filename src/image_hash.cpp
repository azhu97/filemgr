#include "image_hash.hpp"
#include "utils.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <cstdlib>
#include <unordered_set>

namespace {

// Releases a CoreFoundation object when it goes out of scope.
template <typename T>
struct CFHolder {
    T ref;
    explicit CFHolder(T r) : ref(r) {}
    ~CFHolder() { if (ref) CFRelease(ref); }
    CFHolder(const CFHolder&) = delete;
    CFHolder& operator=(const CFHolder&) = delete;
    operator T() const { return ref; }
};

int intProperty(CFDictionaryRef props, CFStringRef key) {
    int value = 0;
    if (auto num = static_cast<CFNumberRef>(CFDictionaryGetValue(props, key))) {
        CFNumberGetValue(num, kCFNumberIntType, &value);
    }
    return value;
}

} // namespace

bool isImageFile(const fs::path& file_path) {
    static const std::unordered_set<std::string> exts = {
        ".jpg", ".jpeg", ".png", ".gif", ".heic", ".heif", ".webp",
        ".tif", ".tiff", ".bmp", ".avif"};
    return exts.count(lowerExtension(file_path)) > 0;
}

int hammingDistance(std::uint64_t a, std::uint64_t b) {
    return __builtin_popcountll(a ^ b);
}

int colorDistance(const ImageFingerprint& a, const ImageFingerprint& b) {
    int total = 0;
    for (std::size_t i = 0; i < a.color.size(); ++i) {
        total += std::abs(int(a.color[i]) - int(b.color[i]));
    }
    return total / static_cast<int>(a.color.size());
}

ImageFingerprint computePerceptualHash(const fs::path& file_path) {
    ImageFingerprint result;

    std::string path = file_path.string();
    CFHolder<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
        nullptr, reinterpret_cast<const UInt8*>(path.data()), static_cast<CFIndex>(path.size()), false));
    if (!url) return result;

    CFHolder<CGImageSourceRef> source(CGImageSourceCreateWithURL(url, nullptr));
    if (!source || CGImageSourceGetCount(source) == 0) return result;

    // Original dimensions (used to prefer the higher-resolution copy).
    CFHolder<CFDictionaryRef> props(CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr));
    if (props) {
        result.width = intProperty(props, kCGImagePropertyPixelWidth);
        result.height = intProperty(props, kCGImagePropertyPixelHeight);
    }

    // Ask ImageIO for a small, orientation-corrected thumbnail. This is much
    // faster than decoding a full 12MP photo just to shrink it to 9x8.
    int max_size = 128;
    CFHolder<CFNumberRef> max_size_ref(CFNumberCreate(nullptr, kCFNumberIntType, &max_size));
    const void* keys[] = {kCGImageSourceCreateThumbnailFromImageAlways,
                          kCGImageSourceCreateThumbnailWithTransform,
                          kCGImageSourceThumbnailMaxPixelSize};
    const void* values[] = {kCFBooleanTrue, kCFBooleanTrue, max_size_ref.ref};
    CFHolder<CFDictionaryRef> options(CFDictionaryCreate(nullptr, keys, values, 3,
                                                         &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks));
    CFHolder<CGImageRef> image(CGImageSourceCreateThumbnailAtIndex(source, 0, options));
    if (!image) return result;

    // Draw into a 9x8 grayscale bitmap on a white background (so transparent
    // regions hash consistently).
    constexpr int W = 9, H = 8;
    unsigned char pixels[W * H] = {};
    CFHolder<CGColorSpaceRef> gray(CGColorSpaceCreateDeviceGray());
    CFHolder<CGContextRef> ctx(CGBitmapContextCreate(pixels, W, H, 8, W, gray, kCGImageAlphaNone));
    if (!ctx) return result;
    CGContextSetGrayFillColor(ctx, 1.0, 1.0);
    CGContextFillRect(ctx, CGRectMake(0, 0, W, H));
    CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
    CGContextDrawImage(ctx, CGRectMake(0, 0, W, H), image);

    // Coarse color signature: 4x4 RGB, again on white.
    constexpr int C = 4;
    unsigned char rgba[C * C * 4] = {};
    CFHolder<CGColorSpaceRef> rgb(CGColorSpaceCreateDeviceRGB());
    CFHolder<CGContextRef> color_ctx(CGBitmapContextCreate(rgba, C, C, 8, C * 4, rgb,
                                                           kCGImageAlphaNoneSkipLast));
    if (!color_ctx) return result;
    CGContextSetRGBFillColor(color_ctx, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(color_ctx, CGRectMake(0, 0, C, C));
    CGContextSetInterpolationQuality(color_ctx, kCGInterpolationHigh);
    CGContextDrawImage(color_ctx, CGRectMake(0, 0, C, C), image);
    for (int i = 0; i < C * C; ++i) {
        for (int ch = 0; ch < 3; ++ch) result.color[i * 3 + ch] = rgba[i * 4 + ch];
    }

    std::uint64_t hash = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W - 1; ++x) {
            hash = (hash << 1) | (pixels[y * W + x] > pixels[y * W + x + 1] ? 1 : 0);
        }
    }
    result.hash = hash;
    result.ok = true;
    return result;
}
