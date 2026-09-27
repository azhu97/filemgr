#include "testing.hpp"
#include "config.hpp"
#include "journal.hpp"
#include "utils.hpp"

namespace {
Context makeContext(const fs::path& root, bool dry_run = false) {
    static Config config = defaultConfig();
    filemgr_directories = config.managedFolders();
    Context ctx;
    ctx.root = root;
    ctx.config = &config;
    ctx.dry_run = dry_run;
    return ctx;
}
} // namespace

TEST(unique_destination_appends_counter) {
    writeFile(tmp() / "a.txt");
    writeFile(tmp() / "a_1.txt");
    CHECK_EQ(uniqueDestination(tmp(), "a.txt"), tmp() / "a_2.txt");
    CHECK_EQ(uniqueDestination(tmp(), "b.txt"), tmp() / "b.txt");
}

TEST(safe_move_never_overwrites) {
    auto ctx = makeContext(tmp());
    writeFile(tmp() / "a.txt", "new");
    writeFile(tmp() / "dir" / "a.txt", "old");
    fs::path dest = safeMove(ctx, tmp() / "a.txt", tmp() / "dir");
    CHECK_EQ(dest, tmp() / "dir" / "a_1.txt");
    CHECK_EQ(readFile(tmp() / "dir" / "a.txt"), std::string("old"));
    CHECK_EQ(readFile(dest), std::string("new"));
    CHECK(!fs::exists(tmp() / "a.txt"));
}

TEST(safe_move_dry_run_touches_nothing) {
    auto ctx = makeContext(tmp(), true);
    writeFile(tmp() / "a.txt");
    fs::path dest = safeMove(ctx, tmp() / "a.txt", tmp() / "dir");
    CHECK_EQ(dest, tmp() / "dir" / "a.txt");
    CHECK(fs::exists(tmp() / "a.txt"));
    CHECK(!fs::exists(tmp() / "dir"));
}

TEST(safe_move_records_journal) {
    auto ctx = makeContext(tmp());
    Journal journal(tmp() / "journal");
    journal.beginRun("sort", tmp());
    ctx.journal = &journal;
    writeFile(tmp() / "a.txt");
    fs::path dest = safeMove(ctx, tmp() / "a.txt", tmp() / "DOCUMENTS");
    auto runs = journal.load();
    CHECK_EQ(runs.size(), 1u);
    CHECK_EQ(runs[0].moves.size(), 1u);
    CHECK_EQ(runs[0].moves[0].first, tmp() / "a.txt");
    CHECK_EQ(runs[0].moves[0].second, dest);
}

TEST(allowed_location_respects_user_folders) {
    makeContext(tmp());
    CHECK(isInAllowedLocation(tmp() / "a.txt", tmp()));
    CHECK(isInAllowedLocation(tmp() / "IMAGES" / "a.png", tmp()));
    CHECK(isInAllowedLocation(tmp() / "IMAGES" / "sub" / "a.png", tmp()));
    CHECK(!isInAllowedLocation(tmp() / "PROTECTED" / "a.png", tmp()));
    CHECK(!isInAllowedLocation(tmp() / "Projects" / "IMAGES" / "a.png", tmp()));
}

TEST(top_level_folder) {
    CHECK_EQ(topLevelFolder(tmp() / "a.txt", tmp()), std::string(""));
    CHECK_EQ(topLevelFolder(tmp() / "DUPLICATES" / "NEAR" / "a.png", tmp()), std::string("DUPLICATES"));
}

TEST(lower_extension_and_hidden) {
    CHECK_EQ(lowerExtension("Photo.JPG"), std::string(".jpg"));
    CHECK_EQ(lowerExtension("README"), std::string(""));
    CHECK(isHidden(".DS_Store"));
    CHECK(!isHidden("a.txt"));
}

TEST(file_hash_matches_known_sha256) {
    writeFile(tmp() / "abc", "abc");
    CHECK_EQ(computeFileHash(tmp() / "abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK_EQ(computeFileHash(tmp() / "missing"), std::string(""));
}

TEST(human_size) {
    CHECK_EQ(ui::humanSize(0), std::string("0 B"));
    CHECK_EQ(ui::humanSize(1536), std::string("1.5 KB"));
    CHECK_EQ(ui::humanSize(5ull * 1024 * 1024 * 1024), std::string("5.0 GB"));
    CHECK_EQ(ui::plural(1, "file"), std::string("1 file"));
    CHECK_EQ(ui::plural(2, "file"), std::string("2 files"));
}

TEST(journal_escapes_awkward_paths) {
    Journal journal(tmp() / "journal");
    journal.beginRun("sort\tx", tmp());
    journal.recordMove(tmp() / "we\tird\nname\\.txt", tmp() / "b");
    journal.markUndone(1, 2);
    auto runs = journal.load();
    CHECK_EQ(runs.size(), 1u);
    CHECK_EQ(runs[0].command, std::string("sort\tx"));
    CHECK_EQ(runs[0].moves[0].first, tmp() / "we\tird\nname\\.txt");
    CHECK_EQ(runs[0].undone_by, 2);
}

TEST(journal_skips_runs_without_moves) {
    Journal journal(tmp() / "journal");
    journal.beginRun("sort", tmp());
    CHECK(!fs::exists(tmp() / "journal"));
    Journal again(tmp() / "journal");
    again.beginRun("dedup", tmp());
    again.recordMove("a", "b");
    CHECK_EQ(again.load().at(0).id, 1);
}
