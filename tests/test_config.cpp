#include "testing.hpp"
#include "config.hpp"

TEST(config_defaults_when_missing) {
    Config c = loadConfig(tmp() / "missing");
    CHECK(c.source.empty());
    CHECK_EQ(c.remote, std::string("gdrive"));
    CHECK_EQ(c.extensionMap().at(".jpg"), std::string("IMAGES"));
    CHECK(c.managedFolders().count("DUPLICATES"));
}

TEST(config_general_keys) {
    writeFile(tmp() / "cfg", "[general]\nremote = work  # comment\nold_days = 7\nrecent_count=3\nthreads = 2\nroot = ~/Stuff\n");
    Config c = loadConfig(tmp() / "cfg");
    CHECK_EQ(c.remote, std::string("work"));
    CHECK_EQ(c.old_days, 7);
    CHECK_EQ(c.recent_count, 3);
    CHECK_EQ(c.threads, 2u);
    CHECK_EQ(c.root, fs::path(getenv("HOME")) / "Stuff");
}

TEST(config_custom_category_wins) {
    writeFile(tmp() / "cfg", "[categories]\nSHEETS = CSV, .xlsx\n");
    Config c = loadConfig(tmp() / "cfg");
    auto map = c.extensionMap();
    CHECK_EQ(map.at(".csv"), std::string("SHEETS"));
    CHECK_EQ(map.at(".xlsx"), std::string("SHEETS"));
    CHECK_EQ(map.at(".pdf"), std::string("DOCUMENTS"));
    CHECK(c.managedFolders().count("SHEETS"));
}

TEST(config_redefine_builtin) {
    writeFile(tmp() / "cfg", "[categories]\nIMAGES = png\n");
    Config c = loadConfig(tmp() / "cfg");
    auto map = c.extensionMap();
    CHECK_EQ(map.at(".png"), std::string("IMAGES"));
    CHECK(map.find(".jpg") == map.end());
}

TEST(config_errors) {
    writeFile(tmp() / "a", "[nope]\n");
    CHECK_THROWS(loadConfig(tmp() / "a"), ConfigError);
    writeFile(tmp() / "b", "[general]\nbogus = 1\n");
    CHECK_THROWS(loadConfig(tmp() / "b"), ConfigError);
    writeFile(tmp() / "c", "[general]\nold_days = soon\n");
    CHECK_THROWS(loadConfig(tmp() / "c"), ConfigError);
    writeFile(tmp() / "d", "[categories]\n../escape = txt\n");
    CHECK_THROWS(loadConfig(tmp() / "d"), ConfigError);
    writeFile(tmp() / "e", "remote = x\n");
    CHECK_THROWS(loadConfig(tmp() / "e"), ConfigError);
}

TEST(config_starter_text_parses) {
    writeFile(tmp() / "cfg", defaultConfigText());
    Config c = loadConfig(tmp() / "cfg");
    CHECK(c.extensionMap() == defaultConfig().extensionMap());
}
