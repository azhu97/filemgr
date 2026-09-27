#include "file_near.hpp"
#include "config.hpp"
#include "image_hash.hpp"
#include <algorithm>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Candidate {
    fs::path path;
    ImageFingerprint fp;
    std::uintmax_t size = 0;
    time_t born = 0;

    long long pixels() const { return static_cast<long long>(fp.width) * fp.height; }
};

// Higher quality first: more pixels, then bigger file, then older.
bool betterCopy(const Candidate& a, const Candidate& b) {
    if (a.pixels() != b.pixels()) return a.pixels() > b.pixels();
    if (a.size != b.size) return a.size > b.size;
    if (a.born != b.born) return a.born < b.born;
    return a.path < b.path;
}

} // namespace

int findNearDuplicates(const Context& ctx, int threshold) {
    const fs::path& root = ctx.root;
    const fs::path near_path = root / "DUPLICATES" / "NEAR";

    ui::info("Looking for near-duplicate images (threshold " + std::to_string(threshold) + "/64)");

    // 1. Collect images in allowed locations, outside DUPLICATES/.
    std::vector<Candidate> candidates;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        const fs::path& p = entry.path();
        if (!entry.is_regular_file() || isHidden(p) || !isImageFile(p)) continue;
        if (!isInAllowedLocation(p, root) || topLevelFolder(p, root) == "DUPLICATES") continue;
        Candidate c;
        c.path = p;
        std::error_code ec;
        c.size = entry.file_size(ec);
        struct stat st;
        if (stat(p.c_str(), &st) == 0) c.born = st.st_birthtimespec.tv_sec;
        candidates.push_back(std::move(c));
    }

    // 2. Hash in parallel with the same queue + worker pool pattern as dedup.
    ThreadSafeQueue<std::size_t> queue;
    unsigned num_threads = ctx.config->threads;
    if (num_threads == 0) num_threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < num_threads; ++i) {
        threads.emplace_back([&] {
            std::size_t index;
            while (queue.pop(index)) {
                candidates[index].fp = computePerceptualHash(candidates[index].path);
            }
        });
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) queue.push(i);
    queue.set_finished();
    for (auto& t : threads) t.join();

    // Drop undecodable images and flat/blank ones (hash 0 carries no
    // information and would match every other blank image).
    std::size_t undecodable = 0;
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                    [&](const Candidate& c) {
                                        if (!c.fp.ok) undecodable++;
                                        return !c.fp.ok || c.fp.hash == 0;
                                    }),
                     candidates.end());
    if (undecodable) ui::detail("skipped " + ui::plural(undecodable, "undecodable image"));

    // 3. Greedy grouping: best copies first; each image joins the first kept
    //    image within the threshold, otherwise it becomes a keeper itself.
    //    Comparing only against keepers avoids chaining (A~B, B~C, A!~C).
    std::sort(candidates.begin(), candidates.end(), betterCopy);
    std::vector<const Candidate*> keepers;
    std::size_t moved = 0;
    std::uintmax_t reclaimed = 0;

    for (const auto& c : candidates) {
        const Candidate* match = nullptr;
        int best_distance = threshold + 1;
        for (const Candidate* k : keepers) {
            int d = hammingDistance(c.fp.hash, k->fp.hash);
            if (d < best_distance && colorDistance(c.fp, k->fp) <= kMaxColorDistance) {
                best_distance = d;
                match = k;
            }
        }
        if (!match) {
            keepers.push_back(&c);
            continue;
        }

        fs::path dest = safeMove(ctx, c.path, near_path);
        if (!dest.empty()) {
            ui::action("near", displayPath(ctx, c.path),
                       displayPath(ctx, dest) + ui::dim("  (distance " + std::to_string(best_distance) +
                                                        " from " + displayPath(ctx, match->path) + ")"));
            moved++;
            reclaimed += c.size;
        }
    }

    ui::detail("compared " + ui::plural(candidates.size(), "image"));
    ui::summary((ctx.dry_run ? "Would move " : "Moved ") + ui::plural(moved, "near-duplicate") + " (" +
                ui::humanSize(reclaimed) + ")");
    return 0;
}
