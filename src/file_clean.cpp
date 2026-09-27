#include "file_clean.hpp"
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool inFolder(const Rule& rule, const std::string& top) {
    if (rule.in.empty()) return true;
    if (rule.in == ".") return top.empty();
    return top == rule.in;
}

std::string describe(const Rule& rule) {
    return rule.action == "trash" ? "trash" : "move to " + rule.target + "/";
}

struct Planned {
    fs::path path;
    std::uintmax_t size;
    const Rule* rule;
};

} // namespace

fs::path trashDirectory() {
    if (const char* t = getenv("FILEMGR_TRASH"); t && *t) return t;
    const char* home = getenv("HOME");
    if (!home) throw std::runtime_error("Could not determine home directory.");
    return fs::path(home) / ".Trash";
}

int cleanWithRules(const Context& ctx, const std::vector<std::string>& only, bool list) {
    const Config& config = *ctx.config;

    std::vector<const Rule*> active;
    for (const auto& name : only) {
        bool found = std::any_of(config.rules.begin(), config.rules.end(),
                                 [&](const Rule& r) { return lower(r.name) == lower(name); });
        if (!found) {
            ui::error("no rule named \"" + name + "\" (see 'filemgr clean --list')");
            return 2;
        }
    }
    for (const auto& r : config.rules) {
        bool selected = only.empty() ? r.enabled
                                     : std::any_of(only.begin(), only.end(),
                                                   [&](const std::string& n) { return lower(n) == lower(r.name); });
        if (selected) active.push_back(&r);
    }

    if (config.rules.empty()) {
        ui::info("No rules configured. Add [rule \"...\"] sections to " +
                 (config.source.empty() ? std::string("your config (filemgr config init)") : config.source.string()) +
                 "; 'filemgr config init' writes commented examples.");
        return 0;
    }

    // Plan first (first matching rule wins), then act, so a rule's moves
    // never feed into another rule during the same run.
    const auto typeMap = config.extensionMap();
    const std::time_t now = std::time(nullptr);
    std::vector<Planned> plan;
    for (const auto& entry : fs::recursive_directory_iterator(ctx.root, fs::directory_options::skip_permission_denied)) {
        const fs::path& p = entry.path();
        std::error_code ec;
        if (!entry.is_regular_file(ec) || isHidden(p) || !isInAllowedLocation(p, ctx.root)) continue;
        auto size = entry.file_size(ec);
        if (ec) continue;
        std::string top = topLevelFolder(p, ctx.root);
        std::time_t modified = modifiedTime(p);
        for (const Rule* r : active) {
            if (inFolder(*r, top) && r->filter.matches(p, size, modified, typeMap, now)) {
                // A move rule never re-moves files already in its target.
                if (r->action == "move" && top == r->target) break;
                plan.push_back({p, size, r});
                break;
            }
        }
    }

    if (list) {
        for (const auto& r : config.rules) {
            std::size_t count = 0;
            std::uintmax_t bytes = 0;
            for (const auto& item : plan) {
                if (item.rule == &r) {
                    count++;
                    bytes += item.size;
                }
            }
            bool is_active = std::find(active.begin(), active.end(), &r) != active.end();
            ui::info(ui::bold(r.name) + (r.enabled ? "" : ui::dim("  (disabled)")) + ui::dim("  → " + describe(r)));
            if (is_active) ui::info("    " + ui::plural(count, "file") + (count == 1 ? " matches" : " match") + " now (" + ui::humanSize(bytes) + ")");
        }
        return 0;
    }

    ui::info("Applying " + ui::plural(active.size(), "rule") + " in " + ui::bold(ctx.root.string()));
    std::size_t done = 0;
    std::uintmax_t bytes = 0;
    const fs::path trash = trashDirectory();
    for (const auto& item : plan) {
        const Rule& r = *item.rule;
        fs::path dest_dir = r.action == "trash" ? trash : ctx.root / r.target;
        fs::path dest = safeMove(ctx, item.path, dest_dir);
        if (dest.empty()) continue;
        std::string shown = r.action == "trash" ? "Trash/" + dest.filename().string() : displayPath(ctx, dest);
        ui::action(r.action, displayPath(ctx, item.path), shown + ui::dim("  [" + r.name + "]"));
        done++;
        bytes += item.size;
    }
    ui::summary((ctx.dry_run ? "Would clean " : "Cleaned ") + ui::plural(done, "file") + " (" +
                ui::humanSize(bytes) + ")" + (done && !ctx.dry_run ? ", undo with 'filemgr undo'" : ""));
    return 0;
}
