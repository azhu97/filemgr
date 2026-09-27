#include "testing.hpp"
#include "cli.hpp"

namespace {

const std::vector<OptionSpec> kGlobals = {
    {"dry-run", 'n', "", ""},
    {"path", 'p', "DIR", ""},
};

std::vector<OptionSpec> commandOpts(const std::string& name) {
    if (name == "upload") return {{"remote", 'r', "NAME", ""}};
    return {};
}

ParsedArgs parse(std::vector<std::string> words) {
    std::vector<char*> argv = {const_cast<char*>("filemgr")};
    for (auto& w : words) argv.push_back(w.data());
    return parseArgs(static_cast<int>(argv.size()), argv.data(), kGlobals, commandOpts);
}

} // namespace

TEST(cli_command_and_positionals) {
    auto a = parse({"recent", "7"});
    CHECK_EQ(a.command, std::string("recent"));
    CHECK_EQ(a.positionals.size(), 1u);
    CHECK_EQ(a.positionals[0], std::string("7"));
}

TEST(cli_options_before_and_after_command) {
    auto a = parse({"-n", "sort", "--path", "/x"});
    CHECK(a.has("dry-run"));
    CHECK_EQ(a.get("path"), std::string("/x"));
    CHECK_EQ(a.command, std::string("sort"));
}

TEST(cli_inline_value) {
    auto a = parse({"sort", "--path=/a=b"});
    CHECK_EQ(a.get("path"), std::string("/a=b"));
}

TEST(cli_command_specific_options) {
    auto a = parse({"upload", "-r", "work", "Folder"});
    CHECK_EQ(a.get("remote"), std::string("work"));
    CHECK_EQ(a.positionals[0], std::string("Folder"));
    // --remote is not valid for other commands
    CHECK_THROWS(parse({"sort", "--remote", "x"}), UsageError);
}

TEST(cli_double_dash_ends_options) {
    auto a = parse({"upload", "--", "-weird-folder"});
    CHECK_EQ(a.positionals[0], std::string("-weird-folder"));
}

TEST(cli_errors) {
    CHECK_THROWS(parse({"--nope"}), UsageError);
    CHECK_THROWS(parse({"sort", "--path"}), UsageError);
    CHECK_THROWS(parse({"--dry-run=yes"}), UsageError);
}

TEST(cli_parse_count) {
    CHECK_EQ(parseCount("42", "n"), 42);
    CHECK_EQ(parseCount("0", "n"), 0);
    CHECK_THROWS(parseCount("-1", "n"), UsageError);
    CHECK_THROWS(parseCount("abc", "n"), UsageError);
    CHECK_THROWS(parseCount("", "n"), UsageError);
    CHECK_THROWS(parseCount("99999999999", "n"), UsageError);
}
