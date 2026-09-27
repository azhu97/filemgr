#include "config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
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
            section = lower(trim(line.substr(1, line.size() - 2)));
            if (section != "general" && section != "categories")
                throw ConfigError(where + ": unknown section [" + section + "]");
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
        } else {
            throw ConfigError(where + ": key outside of a section");
        }
    }
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
           "# SPREADSHEETS = xls xlsx numbers csv\n";
    return out.str();
}
