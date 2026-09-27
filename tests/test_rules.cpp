#include "testing.hpp"
#include <cstdlib>
#include "config.hpp"
#include "file_clean.hpp"
#include "file_history.hpp"
#include "journal.hpp"

namespace {
void age(const fs::path& p, int days) {
    fs::last_write_time(p, fs::file_time_type::clock::now() - std::chrono::hours(24 * days));
}
} // namespace

TEST(rules_parse) {
    writeFile(tmp() / "cfg",
              "[rule \"Old installers\"]\next = dmg, pkg\nolder = 2w\nlarger = 1M\naction = trash\n"
              "[rule Shots]\nmatch = Screenshot*.png \"Screen Shot*\"\nin = .\naction = move SCREENSHOTS\nenabled = no\n");
    Config c = loadConfig(tmp() / "cfg");
    CHECK_EQ(c.rules.size(), 2u);
    const Rule& a = c.rules[0];
    CHECK_EQ(a.name, std::string("Old installers"));
    CHECK_EQ(a.action, std::string("trash"));
    CHECK(a.filter.extensions.count(".pkg"));
    CHECK_EQ(*a.filter.min_age, 14LL * 86400);
    CHECK_EQ(*a.filter.min_size, 1048576u);
    const Rule& b = c.rules[1];
    CHECK_EQ(b.filter.names.size(), 2u);
    CHECK_EQ(b.filter.names[1], std::string("Screen Shot*"));
    CHECK_EQ(b.target, std::string("SCREENSHOTS"));
    CHECK_EQ(b.in, std::string("."));
    CHECK(!b.enabled);
}

TEST(rules_reject_dangerous_or_malformed) {
    writeFile(tmp() / "a", "[rule \"Everything\"]\naction = trash\n");
    CHECK_THROWS(loadConfig(tmp() / "a"), ConfigError);  // no condition
    writeFile(tmp() / "b", "[rule x]\next = txt\n");
    CHECK_THROWS(loadConfig(tmp() / "b"), ConfigError);  // no action
    writeFile(tmp() / "c", "[rule x]\next = txt\naction = delete\n");
    CHECK_THROWS(loadConfig(tmp() / "c"), ConfigError);
    writeFile(tmp() / "d", "[rule x]\next = txt\naction = move ../../etc\n");
    CHECK_THROWS(loadConfig(tmp() / "d"), ConfigError);
    writeFile(tmp() / "e", "[rule x]\nolder = soon\naction = trash\n");
    CHECK_THROWS(loadConfig(tmp() / "e"), ConfigError);
    writeFile(tmp() / "f", "[rule x]\next = a\naction = trash\n[rule x]\next = b\naction = trash\n");
    CHECK_THROWS(loadConfig(tmp() / "f"), ConfigError);  // duplicate name
}

TEST(clean_applies_first_matching_rule_and_undoes) {
    fs::path root = tmp() / "dl";
    setenv("FILEMGR_TRASH", (tmp() / "trash").c_str(), 1);
    writeFile(tmp() / "cfg",
              "[rule \"Old installers\"]\next = dmg\nolder = 7d\naction = trash\n"
              "[rule \"All dmg\"]\next = dmg\naction = move ARCHIVE\n"
              "[rule \"Off\"]\next = txt\naction = trash\nenabled = false\n");
    Config config = loadConfig(tmp() / "cfg");
    filemgr_directories = config.managedFolders();
    writeFile(root / "INSTALLERS" / "old.dmg");
    writeFile(root / "new.dmg");
    writeFile(root / "notes.txt");
    writeFile(root / "PROTECTED" / "old.dmg");
    age(root / "INSTALLERS" / "old.dmg", 30);
    age(root / "PROTECTED" / "old.dmg", 30);

    Journal journal(tmp() / "journal");
    Context ctx;
    ctx.root = root;
    ctx.config = &config;
    ctx.journal = &journal;
    journal.beginRun("clean", root);
    CHECK_EQ(cleanWithRules(ctx, {}, false), 0);

    CHECK(fs::exists(tmp() / "trash" / "old.dmg"));
    CHECK(fs::exists(root / "ARCHIVE" / "new.dmg"));
    CHECK(fs::exists(root / "notes.txt"));             // disabled rule
    CHECK(fs::exists(root / "PROTECTED" / "old.dmg")); // user folder

    journal.beginRun("undo", root);
    CHECK_EQ(undoRun(ctx, 0), 0);
    CHECK(fs::exists(root / "INSTALLERS" / "old.dmg"));
    CHECK(fs::exists(root / "new.dmg"));

    // Naming a disabled rule runs it explicitly.
    journal.beginRun("clean", root);
    cleanWithRules(ctx, {"off"}, false);
    CHECK(fs::exists(tmp() / "trash" / "notes.txt"));
    CHECK(fs::exists(root / "new.dmg"));
}
