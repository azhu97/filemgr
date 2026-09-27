# Watch Daemon

## Problem

Every filemgr command today is a manual, one-shot invocation — the user has to remember to run `sort`, `dedup`, `old`, `recent` themselves. Downloads folders get messy between runs.

## Goal

A `filemgr watch` command that runs as a long-lived process, reacts to filesystem events in `~/Downloads` as they happen, and applies the existing organization logic (starting with `sort`) automatically — no manual invocation, no polling.

## Approach

**FSEvents** (macOS's native filesystem event API, `CoreServices/FSEvents.h`) rather than polling `fs::directory_iterator` on a timer:

1. Register an `FSEventStreamRef` on the Downloads path with a callback that fires on file creation/rename events.
2. Debounce: a download in progress triggers multiple events (temp file created, renamed to final name, sometimes a `.crdownload`/`.download` partial first). Wait for a short quiet period (e.g. 1-2s of no further events on a given path) before acting, so filemgr doesn't grab a half-written file.
3. On settle, run the same per-file logic `sortByType()` already has (extension → type map) against just the new file(s), instead of rescanning the whole directory.
4. Run as a `launchd` user agent (a `.plist` in `~/Library/LaunchAgents/`) so it starts on login and restarts if it crashes — this is the standard macOS way to run a persistent background process, rather than something like a shell `nohup` hack.

## Scope for v1

- Only wire up `sort` — it's the only existing operation that's naturally per-file and safe to run instantly on a new file (no scanning/aggregation across the whole folder like `dedup`/`old`/`recent` do).
- New file `src/file_watch.cpp` / `include/file_watch.hpp`, following the existing one-header-one-function-per-command pattern (`void watchDownloads()`).
- `filemgr watch` runs in the foreground by default (logs to stdout), so it's easy to test with `./filemgr watch` before wiring up `launchd`.
- Respect `isInAllowedLocation()` / `filemgr_directories` exactly as the existing commands do — the daemon must never touch a user's own folders under Downloads, same guarantee as today.
- Provide the `launchd` `.plist` as a template file in the repo (e.g. `launchd/com.filemgr.watch.plist`) with install/uninstall instructions in the README, not auto-installed by the binary itself — installing a login item is a decision the user should take deliberately, not something a CLI command does silently.

## Open questions

- **Debounce window**: how long to wait after the last event on a path before treating a download as "settled"? Too short risks grabbing a partial file; too long delays sorting. Needs testing against real downloads (large files, e.g. a multi-GB `.dmg`, will have gaps between write chunks).
- **Extending beyond `sort`**: should `dedup`/`old` eventually run on a slower interval (e.g. once an hour) from within the same daemon, or stay manual/cron-triggered? They're bulk-scan operations, not naturally per-event, so folding them in isn't a v1 concern.
- **Multiple filemgr instances**: should `watch` take a lock file (e.g. `~/.filemgr.lock`) to prevent two daemons running against the same Downloads folder at once? Probably yes, but not needed until `watch` exists at all.
- **Logging**: stdout is fine for foreground testing, but a `launchd`-managed daemon needs its stdout/stderr redirected to a log file (the `.plist` can do this via `StandardOutPath`/`StandardErrorPath`) — worth deciding the log location/rotation story before shipping the `.plist` template.

## Out of scope for v1

- Linux/`inotify` support — FSEvents is macOS-only, matching the rest of the codebase's existing macOS-only assumption (CommonCrypto).
- A GUI or menu-bar indicator that the daemon is running.
- Auto-installing/managing the `launchd` job from within the `filemgr` binary itself (e.g. a `filemgr watch --install` command) — worth considering later, but v1 ships the `.plist` as a template the user installs manually.
