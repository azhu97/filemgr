#include "testing.hpp"
#include "config.hpp"
#include "stats.hpp"

namespace {
struct StatsFixture {
    Config config = defaultConfig();
    Context ctx;
    explicit StatsFixture(const fs::path& root) {
        fs::create_directories(root);
        filemgr_directories = config.managedFolders();
        ctx.root = root;
        ctx.config = &config;
    }
};

const StatBucket* find(const std::vector<StatBucket>& v, const std::string& name) {
    for (const auto& b : v)
        if (b.name == name) return &b;
    return nullptr;
}
} // namespace

TEST(stats_totals_and_buckets) {
    StatsFixture f(tmp() / "dl");
    writeFile(f.ctx.root / "a.png", std::string(100, 'a'));
    writeFile(f.ctx.root / "IMAGES" / "b.jpg", std::string(300, 'b'));
    writeFile(f.ctx.root / "PROTECTED" / "c.pdf", std::string(50, 'c'));
    writeFile(f.ctx.root / "d.weird", std::string(10, 'd'));
    writeFile(f.ctx.root / ".DS_Store", "hidden");

    FolderStats s = collectStats(f.ctx, 2, false);
    CHECK_EQ(s.total_files, 4u);
    CHECK_EQ(s.total_bytes, 460u);
    CHECK_EQ(find(s.by_category, "IMAGES")->bytes, 400u);
    CHECK_EQ(find(s.by_category, "Other")->count, 1u);
    CHECK_EQ(find(s.by_location, "(top level)")->count, 2u);
    CHECK_EQ(find(s.by_location, "PROTECTED/")->bytes, 50u);
    CHECK_EQ(s.by_category.front().name, std::string("IMAGES"));  // largest first
    CHECK_EQ(s.largest.size(), 2u);
    CHECK_EQ(s.largest[0].bytes, 300u);
    CHECK_EQ(s.unsorted_files, 1u);  // a.png; d.weird has no category
    CHECK(!s.duplicates_scanned);
}

TEST(stats_duplicates_and_stale) {
    StatsFixture f(tmp() / "dl");
    writeFile(f.ctx.root / "a.txt", "same content");
    writeFile(f.ctx.root / "DOCUMENTS" / "b.txt", "same content");
    writeFile(f.ctx.root / "PROTECTED" / "c.txt", "same content");  // not counted: user folder
    writeFile(f.ctx.root / "old.txt", "x");
    fs::last_write_time(f.ctx.root / "old.txt", fs::file_time_type::clock::now() - std::chrono::hours(24 * 100));
    writeFile(f.ctx.root / "PROTECTED" / "old.txt", "y");
    fs::last_write_time(f.ctx.root / "PROTECTED" / "old.txt",
                        fs::file_time_type::clock::now() - std::chrono::hours(24 * 100));

    FolderStats s = collectStats(f.ctx, 10, true);
    CHECK(s.duplicates_scanned);
    CHECK_EQ(s.duplicates.size(), 1u);
    CHECK_EQ(s.duplicate_files, 1u);
    CHECK_EQ(s.duplicate_bytes, 12u);
    CHECK_EQ(s.stale_files, 1u);
    CHECK_EQ(s.by_age.back().count, 0u);  // "Over a year"
    CHECK_EQ(s.by_age[4].count, 2u);      // "3-12 months"
}

TEST(stats_html_is_self_contained_and_escaped) {
    StatsFixture f(tmp() / "dl");
    writeFile(f.ctx.root / "<script>x.png", "img");
    std::string html = renderHtmlReport(collectStats(f.ctx, 5, true));
    CHECK(html.find("<!doctype html>") == 0);
    CHECK(html.find("&lt;script&gt;x.png") != std::string::npos);
    CHECK(html.find("<script>x.png") == std::string::npos);
    CHECK(html.find("http://") == std::string::npos);
    CHECK(html.find("https://") == std::string::npos);
}
