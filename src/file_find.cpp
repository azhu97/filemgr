#include "file_find.hpp"
#include "config.hpp"
#include <algorithm>

namespace fs = std::filesystem;

namespace {

struct Match {
    fs::path path;
    std::uintmax_t size;
    std::time_t modified;
};

std::string formatDate(std::time_t t) {
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return buf;
}

} // namespace

int findFiles(const Context& ctx, const FindOptions& options) {
    fs::path base = options.within.empty() ? ctx.root : ctx.root / options.within;
    if (!fs::is_directory(base)) {
        ui::error(base.string() + " is not a directory");
        return 1;
    }

    const auto typeMap = ctx.config->extensionMap();
    const std::time_t now = std::time(nullptr);
    std::vector<Match> matches;

    auto it = fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied);
    for (auto end = fs::recursive_directory_iterator(); it != end; ++it) {
        const fs::path& p = it->path();
        std::error_code ec;
        if (!options.include_hidden && isHidden(p)) {
            if (it->is_directory(ec)) it.disable_recursion_pending();  // don't descend into .git etc.
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        auto size = it->file_size(ec);
        if (ec) continue;
        std::time_t modified = modifiedTime(p);
        if (options.filter.matches(p, size, modified, typeMap, now)) {
            matches.push_back({p, size, modified});
        }
    }

    if (options.sort == "size") {
        std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.size > b.size; });
    } else if (options.sort == "name") {
        std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) { return a.path < b.path; });
    } else {
        std::sort(matches.begin(), matches.end(),
                  [](const Match& a, const Match& b) { return a.modified > b.modified; });
    }

    std::size_t total = matches.size();
    if (options.limit && matches.size() > options.limit) matches.resize(options.limit);

    if (options.paths_only) {
        for (const auto& m : matches) {
            std::cout << m.path.string() << (options.null_separated ? '\0' : '\n');
        }
        return total ? 0 : 1;
    }

    std::uintmax_t bytes = 0;
    for (const auto& m : matches) {
        bytes += m.size;
        std::string size = ui::humanSize(m.size);
        ui::info("  " + std::string(size.size() < 9 ? 9 - size.size() : 0, ' ') + size + "  " +
                 ui::dim(formatDate(m.modified)) + "  " + displayPath(ctx, m.path));
    }
    std::string shown = matches.size() < total
                            ? " (showing " + std::to_string(matches.size()) + " of " + std::to_string(total) + ")"
                            : "";
    ui::summary(ui::plural(matches.size(), "file") + ", " + ui::humanSize(bytes) + shown);
    return total ? 0 : 1;  // like grep: 1 when nothing matched
}
