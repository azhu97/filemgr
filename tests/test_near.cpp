#include "testing.hpp"
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <functional>
#include <vector>
#include "config.hpp"
#include "file_near.hpp"
#include "image_hash.hpp"

namespace {

using Pixel = std::function<void(int x, int y, unsigned char rgb[3])>;

// Renders an image from a per-pixel function and saves it with ImageIO.
// `uti` is e.g. "public.png" or "public.jpeg".
void writeImage(const fs::path& path, int w, int h, const Pixel& pixel, const char* uti = "public.png") {
    fs::create_directories(path.parent_path());
    std::vector<unsigned char> data(static_cast<size_t>(w) * h * 4, 255);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) pixel(x * 100 / w, y * 100 / h, &data[(y * w + x) * 4]);

    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(data.data(), w, h, 8, w * 4, rgb, kCGImageAlphaNoneSkipLast);
    CGImageRef image = CGBitmapContextCreateImage(ctx);
    std::string p = path.string();
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(p.data()),
                                                           static_cast<CFIndex>(p.size()), false);
    CFStringRef type = CFStringCreateWithCString(nullptr, uti, kCFStringEncodingUTF8);
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, type, 1, nullptr);
    CGImageDestinationAddImage(dest, image, nullptr);
    CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CFRelease(type);
    CFRelease(url);
    CGImageRelease(image);
    CGContextRelease(ctx);
    CGColorSpaceRelease(rgb);
}

// A scene with structure in both axes (x/y in 0-100). `tint` shifts hue.
Pixel scene(int tint_r = 0, int tint_b = 0) {
    return [=](int x, int y, unsigned char rgb[3]) {
        int v = (x * 2 + ((y / 25) % 2) * 60 + (x > 60 && y < 40 ? 80 : 0)) % 256;
        rgb[0] = static_cast<unsigned char>(std::min(255, v + tint_r));
        rgb[1] = static_cast<unsigned char>(v);
        rgb[2] = static_cast<unsigned char>(std::min(255, v / 2 + tint_b));
    };
}

Pixel otherScene() {
    return [](int x, int y, unsigned char rgb[3]) {
        int v = ((100 - y) * 2 + (x < 30 ? 90 : 0)) % 256;
        rgb[0] = rgb[1] = rgb[2] = static_cast<unsigned char>(v);
    };
}

struct NearFixture {
    Config config = defaultConfig();
    Context ctx;
    explicit NearFixture(const fs::path& root) {
        filemgr_directories = config.managedFolders();
        ctx.root = root;
        ctx.config = &config;
    }
};

} // namespace

TEST(phash_resized_and_reencoded_copies_are_close) {
    writeImage(tmp() / "big.png", 400, 300, scene());
    writeImage(tmp() / "small.jpg", 120, 90, scene(), "public.jpeg");
    auto a = computePerceptualHash(tmp() / "big.png");
    auto b = computePerceptualHash(tmp() / "small.jpg");
    CHECK(a.ok);
    CHECK(b.ok);
    CHECK_EQ(a.width, 400);
    CHECK_EQ(a.height, 300);
    CHECK(hammingDistance(a.hash, b.hash) <= kDefaultNearThreshold);
    CHECK(colorDistance(a, b) <= kMaxColorDistance);
}

TEST(phash_different_images_are_far) {
    writeImage(tmp() / "a.png", 200, 200, scene());
    writeImage(tmp() / "b.png", 200, 200, otherScene());
    auto a = computePerceptualHash(tmp() / "a.png");
    auto b = computePerceptualHash(tmp() / "b.png");
    CHECK(hammingDistance(a.hash, b.hash) > kDefaultNearThreshold);
}

TEST(phash_recolored_image_is_rejected_by_color) {
    writeImage(tmp() / "a.png", 200, 200, scene());
    writeImage(tmp() / "b.png", 200, 200, scene(120, 120));
    auto a = computePerceptualHash(tmp() / "a.png");
    auto b = computePerceptualHash(tmp() / "b.png");
    CHECK(colorDistance(a, b) > kMaxColorDistance);
}

TEST(phash_undecodable_file) {
    writeFile(tmp() / "fake.png", "not an image");
    CHECK(!computePerceptualHash(tmp() / "fake.png").ok);
    CHECK(isImageFile("x.HEIC"));
    CHECK(!isImageFile("x.pdf"));
}

TEST(near_keeps_highest_resolution_copy) {
    NearFixture f(tmp() / "dl");
    writeImage(f.ctx.root / "IMAGES" / "full.png", 400, 300, scene());
    writeImage(f.ctx.root / "thumb.jpg", 100, 75, scene(), "public.jpeg");
    writeImage(f.ctx.root / "other.png", 200, 200, otherScene());
    writeImage(f.ctx.root / "PROTECTED" / "copy.png", 200, 150, scene());
    findNearDuplicates(f.ctx, kDefaultNearThreshold);
    CHECK(fs::exists(f.ctx.root / "IMAGES" / "full.png"));
    CHECK(fs::exists(f.ctx.root / "DUPLICATES" / "NEAR" / "thumb.jpg"));
    CHECK(fs::exists(f.ctx.root / "other.png"));
    CHECK(fs::exists(f.ctx.root / "PROTECTED" / "copy.png"));
}

TEST(near_threshold_zero_only_matches_identical_hashes) {
    NearFixture f(tmp() / "dl");
    writeImage(f.ctx.root / "a.png", 200, 200, scene());
    writeImage(f.ctx.root / "b.png", 200, 200, otherScene());
    findNearDuplicates(f.ctx, 0);
    CHECK(fs::exists(f.ctx.root / "a.png"));
    CHECK(fs::exists(f.ctx.root / "b.png"));
}
