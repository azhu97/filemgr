#include "filter.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fnmatch.h>
#include <sstream>
#include <sys/stat.h>

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Splits "12.5M" into (12.5, "m").
std::pair<double, std::string> splitNumber(const std::string& text, const std::string& what) {
    std::string t = lower(text);
    std::size_t i = 0;
    while (i < t.size() && (std::isdigit(static_cast<unsigned char>(t[i])) || t[i] == '.')) ++i;
    if (i == 0) throw FilterError("invalid " + what + " '" + text + "'");
    double value;
    try {
        value = std::stod(t.substr(0, i));
    } catch (const std::exception&) {
        throw FilterError("invalid " + what + " '" + text + "'");
    }
    return {value, t.substr(i)};
}

bool hasWildcard(const std::string& s) { return s.find_first_of("*?[") != std::string::npos; }

} // namespace

std::uintmax_t parseSize(const std::string& text) {
    auto [value, unit] = splitNumber(text, "size");
    if (!unit.empty() && unit.back() == 'b' && unit.size() > 1) unit.pop_back();  // "mb" -> "m"
    double mult;
    if (unit.empty() || unit == "b") mult = 1;
    else if (unit == "k") mult = 1024.0;
    else if (unit == "m") mult = 1024.0 * 1024;
    else if (unit == "g") mult = 1024.0 * 1024 * 1024;
    else if (unit == "t") mult = 1024.0 * 1024 * 1024 * 1024;
    else throw FilterError("invalid size unit in '" + text + "' (use K, M, G or T)");
    return static_cast<std::uintmax_t>(std::llround(value * mult));
}

long long parseAge(const std::string& text) {
    auto [value, unit] = splitNumber(text, "age");
    double mult;
    if (unit == "h") mult = 3600;
    else if (unit.empty() || unit == "d") mult = 86400;
    else if (unit == "w") mult = 7 * 86400;
    else if (unit == "m") mult = 30 * 86400;
    else if (unit == "y") mult = 365 * 86400;
    else throw FilterError("invalid age unit in '" + text + "' (use h, d, w, m or y)");
    return static_cast<long long>(std::llround(value * mult));
}

void FileFilter::addExtensions(const std::string& list) {
    std::string v = list;
    std::replace(v.begin(), v.end(), ',', ' ');
    std::istringstream in(v);
    std::string ext;
    while (in >> ext) {
        ext = lower(ext);
        extensions.insert(ext[0] == '.' ? ext : "." + ext);
    }
}

bool FileFilter::empty() const {
    return names.empty() && extensions.empty() && category.empty() && !min_size && !max_size && !min_age &&
           !max_age;
}

bool FileFilter::matches(const fs::path& path, std::uintmax_t size, std::time_t modified,
                         const std::map<std::string, std::string>& typeMap, std::time_t now) const {
    const std::string ext = lowerExtension(path);
    if (!extensions.empty() && !extensions.count(ext)) return false;

    if (!category.empty()) {
        auto it = typeMap.find(ext);
        std::string cat = it == typeMap.end() ? "Other" : it->second;
        if (lower(cat) != lower(category)) return false;
    }

    if (min_size && size < *min_size) return false;
    if (max_size && size > *max_size) return false;

    long long age = static_cast<long long>(now - modified);
    if (min_age && age < *min_age) return false;
    if (max_age && age > *max_age) return false;

    if (!names.empty()) {
        const std::string name = path.filename().string();
        bool any = std::any_of(names.begin(), names.end(), [&](const std::string& pattern) {
            std::string glob = hasWildcard(pattern) ? pattern : "*" + pattern + "*";
            return fnmatch(glob.c_str(), name.c_str(), FNM_CASEFOLD) == 0;
        });
        if (!any) return false;
    }
    return true;
}

std::time_t modifiedTime(const fs::path& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return 0;
    return st.st_mtimespec.tv_sec;
}
