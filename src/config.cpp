#include "config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

const std::vector<std::pair<std::string, std::string>> kDefaultCategories = {
    {"IMAGES", "jpg jpeg png gif heic heif webp avif bmp tif tiff svg raw"},
    {"VIDEOS", "mp4 mov m4v mkv avi webm mpg mpeg wmv"},
    {"AUDIO", "mp3 wav flac aac m4a ogg aiff"},
    {"DOCUMENTS", "pdf doc docx txt rtf md pages ppt pptx key xls xlsx numbers csv epub"},
    {"COMPRESSED", "zip tar gz tgz bz2 xz 7z rar"},
    {"INSTALLERS", "dmg pkg exe msi iso"},
    {"CODE", "c h cpp hpp cc py js ts jsx tsx html css java rb go rs swift kt sql sh json yaml yml ipynb php"},
};

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::vector<std::string> parseExtensions(const std::string& value) {
    std::string v = value;
    std::replace(v.begin(), v.end(), ',', ' ');
    std::istringstream in(v);
    std::vector<std::string> out;
    std::string ext;
    while (in >> ext) {
        ext = lower(ext);
        if (ext[0] != '.') ext = "." + ext;
        out.push_back(ext);
    }
    return out;
}

int parseInt(const std::string& value, const std::string& where) {
    if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c); }) ||
        value.size() > 9) {
        throw ConfigError(where + ": expected a whole number, got '" + value + "'");
    }
    return std::stoi(value);
}

bool validFolderName(const std::string& name) {
    return !name.empty() && name != "." && name != ".." && name.find('/') == std::string::npos;
}

// Parses `rule "Name"` / `rule Name` section headers; returns false otherwise.
bool parseRuleHeader(const std::string& header, std::string& name) {
    if (header.size() < 5 || lower(header.substr(0, 5)) != "rule " ) return false;
    name = trim(header.substr(5));
    if (name.size() >= 2 && name.front() == '"' && name.back() == '"') name = name.substr(1, name.size() - 2);
    return true;
}

void applyRuleKey(Rule& rule, const std::string& key, const std::string& value, const std::string& where) {
    try {
        if (key == "match") {
            std::istringstream in(value);
            std::string pattern;
            while (in >> std::quoted(pattern)) rule.filter.names.push_back(pattern);
        } else if (key == "ext") {
            rule.filter.addExtensions(value);
        } else if (key == "type") {
            rule.filter.category = value;
        } else if (key == "larger") {
            rule.filter.min_size = parseSize(value);
        } else if (key == "smaller") {
            rule.filter.max_size = parseSize(value);
        } else if (key == "older") {
            rule.filter.min_age = parseAge(value);
        } else if (key == "newer") {
            rule.filter.max_age = parseAge(value);
        } else if (key == "in") {
            if (value != "." && !validFolderName(value))
                throw ConfigError(where + ": 'in' must be a top-level folder name or '.'");
            rule.in = value;
        } else if (key == "enabled") {
            std::string v = lower(value);
            if (v != "true" && v != "false" && v != "yes" && v != "no")
                throw ConfigError(where + ": 'enabled' must be true or false");
            rule.enabled = v == "true" || v == "yes";
        } else if (key == "action") {
            std::istringstream in(value);
            std::string verb, target, extra;
            in >> verb >> target >> extra;
            verb = lower(verb);
            if (verb == "trash" && target.empty()) {
                rule.action = "trash";
            } else if (verb == "move" && validFolderName(target) && extra.empty()) {
                rule.action = "move";
                rule.target = target;
            } else {
                throw ConfigError(where + ": action must be 'trash' or 'move FOLDER'");
            }
        } else {
            throw ConfigError(where + ": unknown key '" + key + "' in rule \"" + rule.name + "\"");
        }
    } catch (const FilterError& e) {
        throw ConfigError(where + ": " + e.what());
    }
}

// Rules without conditions would act on every file; refuse them.
void validateRule(const Rule& rule, const std::string& file) {
    std::string where = file + ":" + std::to_string(rule.line) + ": rule \"" + rule.name + "\"";
    if (rule.action.empty()) throw ConfigError(where + " has no action");
    if (rule.filter.empty()) throw ConfigError(where + " needs at least one condition (match, ext, type, larger, smaller, older, newer)");
}

} // namespace

std::map<std::string, std::string> Config::extensionMap() const {
    std::map<std::string, std::string> map;
    for (const auto& c : categories)
        for (const auto& ext : c.extensions) map[ext] = c.folder;
    return map;
}

std::unordered_set<std::string> Config::managedFolders() const {
    std::unordered_set<std::string> folders = {"DUPLICATES"};
    for (const auto& c : categories) folders.insert(c.folder);
    return folders;
}

Config defaultConfig() {
    Config config;
    for (const auto& [folder, exts] : kDefaultCategories) {
        config.categories.push_back({folder, parseExtensions(exts)});
    }
    return config;
}

fs::path expandHome(const std::string& path) {
    if (path == "~" || path.rfind("~/", 0) == 0) {
        const char* home = getenv("HOME");
        if (home) return fs::path(home) / path.substr(path.size() > 1 ? 2 : 1);
    }
    return fs::path(path);
}

fs::path defaultConfigPath() {
    if (const char* p = getenv("FILEMGR_CONFIG"); p && *p) return p;
    if (const char* x = getenv("XDG_CONFIG_HOME"); x && *x) return fs::path(x) / "filemgr" / "config";
    return expandHome("~/.config/filemgr/config");
}

Config loadConfig(const fs::path& file) {
    Config config = defaultConfig();
    std::ifstream in(file);
    if (!in) return config;  // No file: defaults.
    config.source = file;

    std::string section;
    Rule* rule = nullptr;
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        line_no++;
        std::string where = file.filename().string() + ":" + std::to_string(line_no);

        // Strip comments ("#" or ";" at start, or " #" mid-line).
        if (auto hash = line.find(" #"); hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        if (line.front() == '[') {
            if (line.back() != ']') throw ConfigError(where + ": unterminated section header");
            std::string header = trim(line.substr(1, line.size() - 2));
            std::string rule_name;
            rule = nullptr;
            if (parseRuleHeader(header, rule_name)) {
                if (rule_name.empty()) throw ConfigError(where + ": rule needs a name, e.g. [rule \"Old installers\"]");
                for (const auto& r : config.rules)
                    if (r.name == rule_name) throw ConfigError(where + ": duplicate rule \"" + rule_name + "\"");
                config.rules.push_back(Rule{});
                rule = &config.rules.back();
                rule->name = rule_name;
                rule->line = line_no;
                section = "rule";
                continue;
            }
            section = lower(header);
            if (section != "general" && section != "categories")
                throw ConfigError(where + ": unknown section [" + header + "]");
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) throw ConfigError(where + ": expected 'key = value'");
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        if (section == "general") {
            std::string k = lower(key);
            if (k == "root") config.root = expandHome(value);
            else if (k == "remote") config.remote = value;
            else if (k == "old_days") config.old_days = parseInt(value, where);
            else if (k == "recent_count") config.recent_count = parseInt(value, where);
            else if (k == "threads") config.threads = static_cast<unsigned>(parseInt(value, where));
            else throw ConfigError(where + ": unknown key '" + key + "' in [general]");
        } else if (section == "categories") {
            if (!validFolderName(key)) throw ConfigError(where + ": invalid folder name '" + key + "'");
            auto exts = parseExtensions(value);
            auto it = std::find_if(config.categories.begin(), config.categories.end(),
                                   [&](const Category& c) { return c.folder == key; });
            if (it != config.categories.end()) {
                it->extensions = exts;  // Redefining a folder replaces its list
            } else {
                config.categories.push_back({key, exts});
            }
        } else if (section == "rule") {
            applyRuleKey(*rule, lower(key), value, where);
        } else {
            throw ConfigError(where + ": key outside of a section");
        }
    }
    for (const auto& r : config.rules) validateRule(r, file.filename().string());
    return config;
}

std::string defaultConfigText() {
    std::ostringstream out;
    out << "# filemgr configuration\n"
           "# Lines starting with # are comments. Delete or comment out anything you\n"
           "# don't want to change; missing keys fall back to built-in defaults.\n"
           "\n"
           "[general]\n"
           "# Folder to manage (--path and $FILEMGR_ROOT override this)\n"
           "# root = ~/Downloads\n"
           "\n"
           "# rclone remote used by 'filemgr upload'\n"
           "# remote = gdrive\n"
           "\n"
           "# Defaults for 'filemgr old' and 'filemgr recent'\n"
           "# old_days = 30\n"
           "# recent_count = 5\n"
           "\n"
           "# Hashing threads for dedup (0 = one per CPU core)\n"
           "# threads = 0\n"
           "\n"
           "[categories]\n"
           "# FOLDER = extensions. Redefining a built-in folder replaces its list;\n"
           "# new folders are created and managed like the built-in ones. If an\n"
           "# extension appears twice, the later line wins.\n"
           "#\n"
           "# Built-in defaults:\n";
    for (const auto& [folder, exts] : kDefaultCategories) {
        out << "#   " << folder << " = " << exts << "\n";
    }
    out << "#\n"
           "# Example: send spreadsheets to their own folder\n"
           "# SPREADSHEETS = xls xlsx numbers csv\n"
           "\n"
           "# Cleanup rules, run with 'filemgr clean' (preview with 'filemgr clean -n').\n"
           "# Each rule needs an action and at least one condition. A file gets the\n"
           "# first rule it matches. Rules only see files filemgr manages (top level,\n"
           "# type folders, DUPLICATES), never your own folders.\n"
           "#\n"
           "#   match   = name globs, e.g. Screenshot*.png \"Zoom_*\"\n"
           "#   ext     = extensions, e.g. dmg pkg\n"
           "#   type    = category folder, e.g. INSTALLERS (or Other)\n"
           "#   larger  = minimum size, e.g. 100M     smaller = maximum size\n"
           "#   older   = last modified before, e.g. 14d, 6m, 1y\n"
           "#   newer   = last modified within, e.g. 12h\n"
           "#   in      = only files in this top-level folder ('.' for the top level)\n"
           "#   action  = trash | move FOLDER   (everything goes through the undo journal)\n"
           "#   enabled = false to switch a rule off\n"
           "#\n"
           "# [rule \"Old installers\"]\n"
           "# ext = dmg pkg\n"
           "# older = 14d\n"
           "# action = trash\n"
           "#\n"
           "# [rule \"Stale duplicates\"]\n"
           "# in = DUPLICATES\n"
           "# older = 30d\n"
           "# action = trash\n"
           "#\n"
           "# [rule \"Screenshots\"]\n"
           "# match = Screenshot*.png \"Screen Shot*\"\n"
           "# action = move SCREENSHOTS\n";
    return out.str();
}
