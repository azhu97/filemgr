// Command-level tests: each builds a small fake Downloads folder under tmp().
#include "testing.hpp"
#include <sys/time.h>
#include "config.hpp"
#include "file_dedup.hpp"
#include "file_history.hpp"
#include "file_old.hpp"
#include "file_ops.hpp"
#include "file_recent.hpp"
#include "journal.hpp"

namespace {

struct Fixture {
    Config config = defaultConfig();
    Journal journal;
    Context ctx;

    explicit Fixture(const fs::path& root) : journal(root.parent_path() / (root.filename().string() + ".journal")) {
        fs::create_directories(root);
        filemgr_directories = config.managedFolders();
        ctx.root = root;
        ctx.config = &config;
        ctx.journal = &journal;
    }

    Context& run(const std::string& name) {
        journal.beginRun(name, ctx.root);
        return ctx;
    }
};

// Sets a file's modification time to `days` days ago.
void ageFile(const fs::path& p, int days) {
    auto t = fs::file_time_type::clock::now() - std::chrono::hours(24 * days);
    fs::last_write_time(p, t);
}

} // namespace

TEST(sort_moves_known_types_only) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "a.PNG");
    writeFile(f.ctx.root / "b.pdf");
    writeFile(f.ctx.root / "c.unknown");
    writeFile(f.ctx.root / ".hidden.png");
    writeFile(f.ctx.root / "PROTECTED" / "d.png");
    sortByType(f.run("sort"));
    CHECK(fs::exists(f.ctx.root / "IMAGES" / "a.PNG"));
    CHECK(fs::exists(f.ctx.root / "DOCUMENTS" / "b.pdf"));
    CHECK(fs::exists(f.ctx.root / "c.unknown"));
    CHECK(fs::exists(f.ctx.root / ".hidden.png"));
    CHECK(fs::exists(f.ctx.root / "PROTECTED" / "d.png"));
}

TEST(sort_keeps_existing_file_on_collision) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "IMAGES" / "a.png", "old");
    writeFile(f.ctx.root / "a.png", "new");
    sortByType(f.run("sort"));
    CHECK_EQ(readFile(f.ctx.root / "IMAGES" / "a.png"), std::string("old"));
    CHECK_EQ(readFile(f.ctx.root / "IMAGES" / "a_1.png"), std::string("new"));
}

TEST(sort_uses_custom_categories) {
    Fixture f(tmp() / "dl");
    f.config.categories.push_back({"SHEETS", {".csv"}});
    writeFile(f.ctx.root / "data.csv");
    sortByType(f.run("sort"));
    CHECK(fs::exists(f.ctx.root / "SHEETS" / "data.csv"));
}

TEST(dedup_moves_copies_and_ignores_protected) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "IMAGES" / "a.png", "same");
    writeFile(f.ctx.root / "b.png", "same");
    writeFile(f.ctx.root / "c.png", "diff");
    writeFile(f.ctx.root / "PROTECTED" / "d.png", "same");
    writeFile(f.ctx.root / "empty1");
    writeFile(f.ctx.root / "empty2");
    fs::resize_file(f.ctx.root / "empty1", 0);
    fs::resize_file(f.ctx.root / "empty2", 0);
    deduplicateFiles(f.run("dedup"));

    int remaining = fs::exists(f.ctx.root / "IMAGES" / "a.png") + fs::exists(f.ctx.root / "b.png");
    CHECK_EQ(remaining, 1);
    CHECK_EQ(std::distance(fs::directory_iterator(f.ctx.root / "DUPLICATES"), fs::directory_iterator{}), 1);
    CHECK(fs::exists(f.ctx.root / "c.png"));
    CHECK(fs::exists(f.ctx.root / "PROTECTED" / "d.png"));
    CHECK(fs::exists(f.ctx.root / "empty1"));
    CHECK(fs::exists(f.ctx.root / "empty2"));
}

TEST(dedup_is_idempotent) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "a.txt", "same");
    writeFile(f.ctx.root / "b.txt", "same");
    deduplicateFiles(f.run("dedup"));
    deduplicateFiles(f.run("dedup"));
    CHECK_EQ(std::distance(fs::directory_iterator(f.ctx.root / "DUPLICATES"), fs::directory_iterator{}), 1);
}

TEST(old_archives_stale_files_and_restores_fresh_ones) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "stale.txt");
    writeFile(f.ctx.root / "fresh.txt");
    writeFile(f.ctx.root / "PROTECTED" / "stale.txt");
    ageFile(f.ctx.root / "stale.txt", 40);
    ageFile(f.ctx.root / "PROTECTED" / "stale.txt", 40);
    archiveOld(f.run("old"), 30);
    CHECK(fs::exists(f.ctx.root / "OLD" / "stale.txt"));
    CHECK(fs::exists(f.ctx.root / "fresh.txt"));
    CHECK(fs::exists(f.ctx.root / "PROTECTED" / "stale.txt"));

    // With a longer threshold the file is no longer "old" and comes back.
    archiveOld(f.run("old"), 60);
    CHECK(fs::exists(f.ctx.root / "stale.txt"));
    CHECK(!fs::exists(f.ctx.root / "OLD" / "stale.txt"));
}

TEST(recent_surfaces_newest_but_not_duplicates) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "IMAGES" / "old.png");
    writeFile(f.ctx.root / "IMAGES" / "new.png");
    writeFile(f.ctx.root / "DUPLICATES" / "newest.png");
    ageFile(f.ctx.root / "IMAGES" / "old.png", 10);
    ageFile(f.ctx.root / "IMAGES" / "new.png", 1);
    recentFile(f.run("recent"), 1);
    CHECK(fs::exists(f.ctx.root / "new.png"));
    CHECK(fs::exists(f.ctx.root / "IMAGES" / "old.png"));
    CHECK(fs::exists(f.ctx.root / "DUPLICATES" / "newest.png"));
}

TEST(undo_restores_and_redo_reapplies) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "a.png");
    writeFile(f.ctx.root / "b.pdf");
    sortByType(f.run("sort"));
    CHECK(fs::exists(f.ctx.root / "IMAGES" / "a.png"));

    CHECK_EQ(undoRun(f.run("undo"), 0), 0);
    CHECK(fs::exists(f.ctx.root / "a.png"));
    CHECK(fs::exists(f.ctx.root / "b.pdf"));
    CHECK_EQ(f.journal.load().at(0).undone_by, 2);

    // Undo of the undo = redo
    CHECK_EQ(undoRun(f.run("undo"), 0), 0);
    CHECK(fs::exists(f.ctx.root / "IMAGES" / "a.png"));

    // Run 1 is already undone and can't be undone twice
    CHECK_EQ(undoRun(f.run("undo"), 1), 1);
}

TEST(undo_does_not_clobber_new_files) {
    Fixture f(tmp() / "dl");
    writeFile(f.ctx.root / "a.png", "sorted");
    sortByType(f.run("sort"));
    writeFile(f.ctx.root / "a.png", "newcomer");
    undoRun(f.run("undo"), 0);
    CHECK_EQ(readFile(f.ctx.root / "a.png"), std::string("newcomer"));
    CHECK_EQ(readFile(f.ctx.root / "a_1.png"), std::string("sorted"));
}
