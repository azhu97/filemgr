#include "file_watch.hpp"
#include "config.hpp"
#include "file_ops.hpp"
#include "journal.hpp"
#include <CoreServices/CoreServices.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <map>
#include <mutex>
#include <set>
#include <sys/file.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

std::atomic<bool> g_stop{false};

void handleSignal(int) { g_stop = true; }

// Extensions browsers use while a download is still in progress. The final
// rename to the real name produces its own event, which is what gets sorted.
bool isPartialDownload(const fs::path& p) {
    static const std::set<std::string> partial = {".crdownload", ".download", ".part", ".partial",
                                                  ".tmp", ".opdownload", ".!qb"};
    return partial.count(lowerExtension(p)) > 0;
}

std::string timestamp() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

void log(const std::string& message) { ui::info(ui::dim("[" + timestamp() + "]") + " " + message); }

struct Pending {
    Clock::time_point last_event;
    std::uintmax_t last_size = 0;
    bool size_checked = false;
};

// State shared between the FSEvents dispatch queue and the main loop.
struct WatchState {
    fs::path root;
    std::mutex mutex;
    std::map<fs::path, Pending> pending;
    bool rescan = false;

    void touch(const fs::path& p) {
        std::lock_guard<std::mutex> lock(mutex);
        auto& entry = pending[p];
        entry.last_event = Clock::now();
        entry.size_checked = false;
    }
};

void onEvents(ConstFSEventStreamRef, void* info, size_t count, void* paths,
              const FSEventStreamEventFlags flags[], const FSEventStreamEventId[]) {
    auto* state = static_cast<WatchState*>(info);
    auto** event_paths = static_cast<char**>(paths);
    for (size_t i = 0; i < count; ++i) {
        if (flags[i] & (kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagRootChanged)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->rescan = true;
            continue;
        }
        if (!(flags[i] & kFSEventStreamEventFlagItemIsFile)) continue;
        fs::path p(event_paths[i]);
        // Only files landing directly in the root are sorted; anything in a
        // subfolder is either already sorted or in a user folder.
        if (p.parent_path() != state->root) continue;
        state->touch(p);
    }
}

// Holds an exclusive lock on <state>/watch.lock for the process lifetime.
class LockFile {
public:
    explicit LockFile(const fs::path& path) {
        fs::create_directories(path.parent_path());
        fd_ = open(path.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd_ >= 0 && flock(fd_, LOCK_EX | LOCK_NB) != 0) {
            close(fd_);
            fd_ = -1;
            return;
        }
        if (fd_ >= 0) {
            std::string pid = std::to_string(getpid()) + "\n";
            if (ftruncate(fd_, 0) == 0 && write(fd_, pid.data(), pid.size()) < 0) {
                // Best effort: the PID is informational only.
            }
        }
    }
    ~LockFile() {
        if (fd_ >= 0) close(fd_);  // Closing releases the flock
    }
    bool held() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

// Moves settled files into their type folders; returns how many moved.
std::size_t processSettled(const Context& ctx, WatchState& state, const std::map<std::string, std::string>& typeMap,
                           Clock::duration settle) {
    std::vector<fs::path> ready;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        auto now = Clock::now();
        for (auto it = state.pending.begin(); it != state.pending.end();) {
            const fs::path& p = it->first;
            std::error_code ec;
            if (!fs::is_regular_file(p, ec) || isHidden(p) || isPartialDownload(p)) {
                it = state.pending.erase(it);
                continue;
            }
            Pending& entry = it->second;
            if (now - entry.last_event < settle) {
                ++it;
                continue;
            }
            // Require the size to stay the same across one more settle window,
            // so large downloads that pause between chunks aren't grabbed early.
            auto size = fs::file_size(p, ec);
            if (ec || !entry.size_checked || size != entry.last_size) {
                entry.last_size = size;
                entry.size_checked = true;
                entry.last_event = now;
                ++it;
                continue;
            }
            ready.push_back(p);
            it = state.pending.erase(it);
        }
    }

    if (ready.empty()) return 0;

    // Each batch is its own journal run, so `undo` reverses the latest batch.
    if (ctx.journal) ctx.journal->beginRun("watch", ctx.root);
    std::size_t moved = 0;
    for (const auto& p : ready) {
        fs::path dest = sortOneFile(ctx, p, typeMap, /*report=*/false);
        if (!dest.empty()) {
            log("sorted " + p.filename().string() + " -> " + displayPath(ctx, dest));
            moved++;
        }
    }
    return moved;
}

std::string executablePath() {
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return "filemgr";
    std::error_code ec;
    fs::path p = fs::canonical(buf, ec);
    return ec ? std::string(buf) : p.string();
}

std::string xmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

} // namespace

int watchDownloads(const Context& base_ctx, const WatchOptions& options) {
    // FSEvents reports resolved paths (/private/var/... rather than /var/...),
    // so work with the canonical root throughout.
    Context ctx = base_ctx;
    ctx.root = fs::canonical(base_ctx.root);

    LockFile lock(stateDirectory() / "watch.lock");
    if (!lock.held()) {
        ui::error("another 'filemgr watch' is already running (lock: " +
                  (stateDirectory() / "watch.lock").string() + ")");
        return 1;
    }

    const auto typeMap = ctx.config->extensionMap();
    WatchState state;
    state.root = ctx.root;

    if (options.sort_existing) {
        sortByType(ctx);
    }

    std::string root_str = ctx.root.string();
    CFStringRef cf_root = CFStringCreateWithCString(nullptr, root_str.c_str(), kCFStringEncodingUTF8);
    CFArrayRef paths = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&cf_root), 1, &kCFTypeArrayCallBacks);
    FSEventStreamContext stream_ctx = {0, &state, nullptr, nullptr, nullptr};
    FSEventStreamRef stream = FSEventStreamCreate(
        nullptr, &onEvents, &stream_ctx, paths, kFSEventStreamEventIdSinceNow, 0.3,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot);
    CFRelease(paths);
    CFRelease(cf_root);
    if (!stream) {
        ui::error("could not create FSEvents stream for " + root_str);
        return 1;
    }

    dispatch_queue_t queue = dispatch_queue_create("filemgr.watch", DISPATCH_QUEUE_SERIAL);
    FSEventStreamSetDispatchQueue(stream, queue);
    if (!FSEventStreamStart(stream)) {
        ui::error("could not start FSEvents stream");
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        return 1;
    }

    struct sigaction sa = {};
    sa.sa_handler = handleSignal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    log("watching " + ui::bold(root_str) + (ctx.dry_run ? ui::yellow(" (dry run)") : "") + ", press Ctrl-C to stop");
    auto settle = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.settle_seconds));
    std::size_t total = 0;

    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        bool rescan = false;
        {
            std::lock_guard<std::mutex> guard(state.mutex);
            std::swap(rescan, state.rescan);
        }
        if (rescan) {
            // Events were dropped; queue every top-level file for a settle check.
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(ctx.root, ec)) state.touch(entry.path());
        }

        total += processSettled(ctx, state, typeMap, settle);
    }

    FSEventStreamStop(stream);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
    dispatch_release(queue);

    log("stopped; sorted " + ui::plural(total, "file") + " this session");
    return 0;
}

int printLaunchdPlist(const Context& ctx) {
    const char* home = getenv("HOME");
    std::string log_path = std::string(home ? home : "/tmp") + "/Library/Logs/filemgr-watch.log";
    std::cout << R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.filemgr.watch</string>
    <key>ProgramArguments</key>
    <array>
        <string>)" << xmlEscape(executablePath()) << R"(</string>
        <string>watch</string>
        <string>--path</string>
        <string>)" << xmlEscape(ctx.root.string()) << R"(</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <dict>
        <key>SuccessfulExit</key>
        <false/>
    </dict>
    <key>ProcessType</key>
    <string>Background</string>
    <key>StandardOutPath</key>
    <string>)" << xmlEscape(log_path) << R"(</string>
    <key>StandardErrorPath</key>
    <string>)" << xmlEscape(log_path) << R"(</string>
</dict>
</plist>
)";
    return 0;
}
