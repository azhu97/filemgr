#include "file_history.hpp"
#include <algorithm>

namespace fs = std::filesystem;

namespace {

std::string formatTime(std::time_t t) {
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

// Shows a path relative to the run's root when possible.
std::string rel(const JournalRun& run, const fs::path& p) {
    fs::path r = p.lexically_relative(run.root);
    return (r.empty() || *r.begin() == "..") ? p.string() : r.string();
}

} // namespace

int showHistory(const Journal& journal, int limit, int run_id) {
    auto runs = journal.load();

    if (run_id != 0) {
        auto it = std::find_if(runs.begin(), runs.end(), [&](const JournalRun& r) { return r.id == run_id; });
        if (it == runs.end()) {
            ui::error("no run #" + std::to_string(run_id) + " in the journal");
            return 1;
        }
        ui::info(ui::bold("#" + std::to_string(it->id)) + "  " + formatTime(it->when) + "  " +
                 ui::cyan(it->command) + "  " + ui::dim(it->root.string()));
        if (it->undone_by) ui::info(ui::yellow("  undone by #" + std::to_string(it->undone_by)));
        for (const auto& [from, to] : it->moves) {
            ui::info("  " + rel(*it, from) + ui::dim(" -> ") + rel(*it, to));
        }
        return 0;
    }

    if (runs.empty()) {
        ui::info("No history yet. Runs that move files are recorded in " + journal.file().string());
        return 0;
    }

    std::size_t start = runs.size() > static_cast<std::size_t>(limit) ? runs.size() - limit : 0;
    for (std::size_t i = runs.size(); i-- > start;) {
        const auto& r = runs[i];
        std::string line = ui::bold("#" + std::to_string(r.id)) + "  " + formatTime(r.when) + "  " +
                           ui::cyan(r.command) + "  " + ui::plural(r.moves.size(), "move");
        if (r.undone_by) line += ui::yellow("  (undone by #" + std::to_string(r.undone_by) + ")");
        ui::info(line);
    }
    ui::info(ui::dim("Show a run's moves with 'filemgr history <id>'; reverse one with 'filemgr undo [id]'."));
    return 0;
}

int undoRun(const Context& ctx, int run_id) {
    Journal& journal = *ctx.journal;
    auto runs = journal.load();

    const JournalRun* target = nullptr;
    for (auto it = runs.rbegin(); it != runs.rend(); ++it) {
        // The current undo run itself has no moves yet, so it never matches.
        if (run_id == 0 ? (!it->undone_by && !it->moves.empty()) : it->id == run_id) {
            target = &*it;
            break;
        }
    }
    if (!target) {
        ui::error(run_id ? "no run #" + std::to_string(run_id) + " in the journal" : "nothing to undo");
        return 1;
    }
    if (target->undone_by) {
        ui::error("run #" + std::to_string(target->id) + " was already undone by #" +
                  std::to_string(target->undone_by) + " (undo that run to redo it)");
        return 1;
    }

    ui::info("Undoing #" + std::to_string(target->id) + " (" + target->command + ", " +
             formatTime(target->when) + ")");

    std::size_t restored = 0, skipped = 0;
    for (auto it = target->moves.rbegin(); it != target->moves.rend(); ++it) {
        const auto& [from, to] = *it;
        if (!fs::exists(fs::symlink_status(to))) {
            ui::warn(rel(*target, to) + " no longer exists; skipping");
            skipped++;
            continue;
        }
        fs::path dest = safeMoveTo(ctx, to, from);
        if (dest.empty()) {
            skipped++;
            continue;
        }
        std::string shown = rel(*target, dest);
        if (dest != from) shown += ui::yellow("  (" + from.filename().string() + " was taken)");
        ui::action("undo", rel(*target, to), shown);
        restored++;
    }

    if (!ctx.dry_run && restored > 0) {
        journal.markUndone(target->id, journal.currentRunId());
    }
    ui::summary((ctx.dry_run ? "Would restore " : "Restored ") + ui::plural(restored, "file") +
                (skipped ? ", skipped " + std::to_string(skipped) : ""));
    return skipped && !restored ? 1 : 0;
}
