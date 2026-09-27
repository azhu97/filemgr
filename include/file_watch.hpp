#pragma once
#include "utils.hpp"
#include <string>

struct WatchOptions {
    double settle_seconds = 2.0;  // Quiet period before a new file is sorted
    bool sort_existing = false;   // Sort files already present at startup
};

// `filemgr watch`: sorts new files as they land in the managed folder, using
// FSEvents. Runs in the foreground until SIGINT/SIGTERM. Only one watcher can
// run at a time (enforced with a lock file in the state directory).
int watchDownloads(const Context& ctx, const WatchOptions& options);

// Prints a launchd agent plist that runs `filemgr watch` at login.
int printLaunchdPlist(const Context& ctx);
