#include "stats.hpp"
#include "config.hpp"
#include "duplicates.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace {

struct AgeBand {
    const char* name;
    long long max_days;
};

const AgeBand kAgeBands[] = {
    {"Today", 1}, {"This week", 7}, {"This month", 30}, {"1-3 months", 90},
    {"3-12 months", 365}, {"Over a year", -1},
};

std::time_t toTimeT(fs::file_time_type t) {
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sys);
}

std::vector<StatBucket> sortedBuckets(const std::map<std::string, StatBucket>& m) {
    std::vector<StatBucket> v;
    for (const auto& [name, b] : m) v.push_back(b);
    std::sort(v.begin(), v.end(), [](const StatBucket& a, const StatBucket& b) { return a.bytes > b.bytes; });
    return v;
}

time_t birthTime(const fs::path& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? st.st_birthtimespec.tv_sec : 0;
}

// Text bar of `width` cells proportional to value/max, using eighth blocks.
std::string bar(std::uintmax_t value, std::uintmax_t max, int width) {
    static const char* partial[] = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    if (max == 0) return std::string(width, ' ');
    int eighths = static_cast<int>(static_cast<double>(value) / max * width * 8 + 0.5);
    std::string out;
    int cells = 0;
    for (; eighths >= 8; eighths -= 8, ++cells) out += "█";
    if (eighths > 0) {
        out += partial[eighths];
        ++cells;
    }
    return out + std::string(std::max(0, width - cells), ' ');
}

std::string pad(const std::string& s, std::size_t width) {
    return s.size() >= width ? s : s + std::string(width - s.size(), ' ');
}

std::string padLeft(const std::string& s, std::size_t width) {
    return s.size() >= width ? s : std::string(width - s.size(), ' ') + s;
}

void printBuckets(const std::string& title, const std::vector<StatBucket>& buckets, std::uintmax_t total) {
    ui::info("");
    ui::info(ui::bold(title));
    std::uintmax_t max = 0;
    std::size_t name_width = 0;
    for (const auto& b : buckets) {
        max = std::max(max, b.bytes);
        name_width = std::max(name_width, b.name.size());
    }
    for (const auto& b : buckets) {
        if (b.count == 0) continue;
        int pct = total ? static_cast<int>(100.0 * b.bytes / total + 0.5) : 0;
        ui::info("  " + pad(b.name, name_width) + "  " + ui::cyan(bar(b.bytes, max, 24)) + "  " +
                 padLeft(ui::humanSize(b.bytes), 9) + padLeft(std::to_string(pct) + "%", 5) +
                 ui::dim("  " + ui::plural(b.count, "file")));
    }
}

} // namespace

FolderStats collectStats(const Context& ctx, std::size_t top_n, bool scan_duplicates) {
    FolderStats s;
    s.root = ctx.root;
    s.generated = std::time(nullptr);
    s.stale_days = ctx.config->old_days;

    const auto typeMap = ctx.config->extensionMap();
    std::map<std::string, StatBucket> categories, locations;
    std::vector<StatBucket> ages;
    for (const auto& band : kAgeBands) ages.push_back({band.name, 0, 0});

    const auto now = std::time(nullptr);
    std::vector<FileEntry> files;

    for (const auto& entry : fs::recursive_directory_iterator(ctx.root, fs::directory_options::skip_permission_denied)) {
        const fs::path& p = entry.path();
        std::error_code ec;
        if (!entry.is_regular_file(ec) || entry.is_symlink(ec) || isHidden(p)) continue;
        FileEntry f{p, entry.file_size(ec), 0};
        if (ec) continue;
        auto mtime = entry.last_write_time(ec);
        f.modified = ec ? now : toTimeT(mtime);

        s.total_files++;
        s.total_bytes += f.bytes;

        auto cat = typeMap.find(lowerExtension(p));
        std::string cat_name = cat == typeMap.end() ? "Other" : cat->second;
        auto& cb = categories[cat_name];
        cb.name = cat_name;
        cb.count++;
        cb.bytes += f.bytes;

        std::string top = topLevelFolder(p, ctx.root);
        std::string loc_name = top.empty() ? "(top level)" : top + "/";
        auto& lb = locations[loc_name];
        lb.name = loc_name;
        lb.count++;
        lb.bytes += f.bytes;

        long long age_days = (now - f.modified) / 86400;
        for (std::size_t i = 0; i < ages.size(); ++i) {
            if (kAgeBands[i].max_days < 0 || age_days < kAgeBands[i].max_days) {
                ages[i].count++;
                ages[i].bytes += f.bytes;
                break;
            }
        }

        if (isInAllowedLocation(p, ctx.root) && top != "DUPLICATES" && top != "OLD" &&
            age_days >= s.stale_days) {
            s.stale_files++;
            s.stale_bytes += f.bytes;
        }
        if (top.empty() && cat != typeMap.end()) s.unsorted_files++;

        files.push_back(std::move(f));
    }

    s.by_category = sortedBuckets(categories);
    s.by_location = sortedBuckets(locations);
    s.by_age = ages;

    std::size_t n = std::min(top_n, files.size());
    std::partial_sort(files.begin(), files.begin() + n, files.end(),
                      [](const FileEntry& a, const FileEntry& b) { return a.bytes > b.bytes; });
    s.largest.assign(files.begin(), files.begin() + n);

    if (scan_duplicates) {
        s.duplicates_scanned = true;
        for (auto& group : findExactDuplicates(dedupCandidates(ctx.root), ctx.config->threads)) {
            std::sort(group.begin(), group.end(), [](const fs::path& a, const fs::path& b) {
                time_t ta = birthTime(a), tb = birthTime(b);
                return ta != tb ? ta < tb : a < b;
            });
            DuplicateGroup g;
            std::error_code ec;
            g.file_bytes = fs::file_size(group.front(), ec);
            g.paths = std::move(group);
            s.duplicate_files += g.paths.size() - 1;
            s.duplicate_bytes += g.wasted();
            s.duplicates.push_back(std::move(g));
        }
        std::sort(s.duplicates.begin(), s.duplicates.end(),
                  [](const DuplicateGroup& a, const DuplicateGroup& b) { return a.wasted() > b.wasted(); });
    }
    return s;
}

int showStats(const Context& ctx, std::size_t top_n, bool scan_duplicates, const std::string& html_path) {
    FolderStats s = collectStats(ctx, top_n, scan_duplicates);

    if (!html_path.empty()) {
        fs::path out = expandHome(html_path);
        std::ofstream file(out);
        if (!file) {
            ui::error("cannot write " + out.string());
            return 1;
        }
        file << renderHtmlReport(s);
        ui::summary("Wrote report to " + fs::absolute(out).string());
        return 0;
    }

    ui::info(ui::bold(s.root.string()) + "  " + ui::plural(s.total_files, "file") + ", " +
             ui::humanSize(s.total_bytes));

    printBuckets("By type", s.by_category, s.total_bytes);
    printBuckets("By folder", s.by_location, s.total_bytes);
    printBuckets("By last modified", s.by_age, s.total_bytes);

    if (!s.largest.empty()) {
        ui::info("");
        ui::info(ui::bold("Largest files"));
        for (const auto& f : s.largest) {
            ui::info("  " + padLeft(ui::humanSize(f.bytes), 9) + "  " + displayPath(ctx, f.path));
        }
    }

    ui::info("");
    ui::info(ui::bold("Cleanup opportunities"));
    if (s.duplicates_scanned) {
        ui::info("  " + std::to_string(s.duplicate_files) +
                 (s.duplicate_files == 1 ? " duplicate copy" : " duplicate copies") + " wasting " +
                 ui::yellow(ui::humanSize(s.duplicate_bytes)) + ui::dim("   filemgr dedup"));
        for (std::size_t i = 0; i < s.duplicates.size() && i < 5; ++i) {
            const auto& g = s.duplicates[i];
            ui::info(ui::dim("    " + ui::humanSize(g.wasted()) + " in " + std::to_string(g.paths.size()) +
                             " copies of " + displayPath(ctx, g.paths.front())));
        }
    }
    ui::info("  " + ui::plural(s.stale_files, "file") + " untouched for " + std::to_string(s.stale_days) +
             "+ days (" + ui::yellow(ui::humanSize(s.stale_bytes)) + ")" + ui::dim("   filemgr old"));
    ui::info("  " + ui::plural(s.unsorted_files, "unsorted file") + " at the top level" + ui::dim("   filemgr sort"));
    return 0;
}
