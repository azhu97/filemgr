# Changelog

All notable changes to filemgr. Each entry corresponds to a feature branch
merged into `main`.

## Unreleased

### Rules engine (`feature/rules-engine`)
- `[rule "Name"]` sections in the config: conditions `match`, `ext`, `type`,
  `larger`, `smaller`, `older`, `newer`, `in`, plus `action = trash | move FOLDER`
  and `enabled`.
- New `clean [rule...]` command applies enabled rules (or the named ones);
  `--list` shows live match counts. Supports `--dry-run`.
- `trash` moves to `~/.Trash` (override: `$FILEMGR_TRASH`) and is journaled,
  so `undo` restores trashed files.
- Safety: rules only act on filemgr-managed files, need at least one
  condition, and cannot target paths outside the managed folder.

### Search (`feature/search`)
- New read-only `find` command: name globs or substrings, `--type`, `--ext`,
  `--larger`/`--smaller`, `--older`/`--newer`, `--in FOLDER`, `--sort`,
  `--limit`, `--hidden`, and `--paths`/`--print0` for piping.
- New `FileFilter` module (`filter.{hpp,cpp}`) with human-friendly size (`1.5G`)
  and age (`2w`, `6m`) parsing, reused by the rules engine.

### Stats and reports (`feature/stats-report`)
- New read-only `stats` command: usage by type, folder and last-modified age,
  the largest files, and cleanup opportunities (duplicate waste, stale files,
  unsorted files) with the command that addresses each.
- `stats --html FILE` writes a self-contained HTML report (light/dark,
  hover tooltips, works at phone width, no network access).
- Exact-duplicate search moved into `duplicates.{hpp,cpp}` and shared by
  `dedup` and `stats`. `dedup` output is now deterministic.

### Watch daemon (`feature/watch-daemon`)
- New `watch` command sorts new files as they arrive, using FSEvents.
  Waits for files to settle (`--settle`, default 2s, plus a size-stability
  check) and ignores partial downloads (`.crdownload`, `.part`, ...).
- `--sort-existing` sorts the current contents at startup.
- Single-instance lock (`~/.filemgr/watch.lock`); clean shutdown on SIGINT/SIGTERM.
- Each sorted batch is a journal run, so `undo` works on watch activity.
- `--print-plist` generates a launchd agent; a template is in `launchd/`.
- `sort` logic refactored into `sortOneFile()`, shared with `watch`.
- Build now links `CoreServices`.

### Near-duplicate detection (`feature/near-duplicate-detection`)
- `dedup --near` finds visually similar images (resized, re-compressed or
  format-converted copies) and moves them into `DUPLICATES/NEAR/`, keeping the
  highest-resolution copy. `--threshold N` (0-64, default 6) tunes sensitivity.
- Fingerprints combine a 64-bit dHash with a 4x4 color grid, so recolored
  variants are not flagged.
- Images are decoded with the macOS ImageIO framework (HEIC, WebP, AVIF, ...);
  the build now links `CoreGraphics` and `ImageIO`.
- Design doc updated with implementation notes and measured thresholds.

### Test suite (`feature/test-suite`)
- `make test` builds and runs unit tests (`build/unit_tests`) and end-to-end
  tests (`tests/e2e.sh`). Neither touches your real Downloads, state or config.
- Tiny dependency-free framework in `tests/testing.hpp`.
- Coverage: argument parsing, config parsing/validation, collision-safe moves,
  dry runs, journal escaping, protected folders, and every file command
  including undo/redo.

### Configuration (`feature/config`)
- New INI-style config file at `~/.config/filemgr/config` (`--config FILE`,
  `$FILEMGR_CONFIG` and `$XDG_CONFIG_HOME` are respected).
- `[general]`: `root`, `remote`, `old_days`, `recent_count`, `threads`.
- `[categories]`: add new type folders or redefine built-in ones; custom
  folders are managed (and protected-folder rules apply) like the built-ins.
- Many more built-in extensions (webp, avif, mov, flac, xlsx, 7z, pkg, json, ...).
- New `config` command: `show` (default), `path`, `init [--force]`, `edit`.
- Hard-coded `gdrive` remote is now just the default.

### Undo journal (`feature/undo-journal`)
- Every move made by `sort`, `recent`, `dedup`, `old` and `undo` is appended
  to `~/.filemgr/journal` (or `$FILEMGR_STATE_DIR/journal`) as it happens.
- New `history [id]` command lists runs or the moves of one run.
- New `undo [id]` command reverses a run; undoing an undo re-applies it.
- `--no-journal` skips recording for a single run.
- New `safeMoveTo()` helper moves a file to an exact path (collision-safe).
- Journal format is documented in `include/journal.hpp`.

### CLI overhaul (`feature/cli-overhaul`)
- New argument parser: long/short options anywhere on the command line,
  `--help` globally and per command, `--version`.
- Global `--dry-run`, `--path DIR`, `--verbose`, `--quiet`, `--no-color`, `--time`.
  `$FILEMGR_ROOT` overrides the default folder.
- Colored, consistent output with a summary line per command (for example
  "Moved 3 duplicates (12.4 MB)"). Execution time now only prints with `--time`/`--verbose`.
- Invalid arguments (`old abc`) exit with status 2 and a message instead of crashing.
- `dedup` only hashes files that share a size with another file (much faster
  on large folders), skips empty files, and reports reclaimed space.
- `recent` no longer pulls files back out of `DUPLICATES/`.
- `old` collects stale files before moving them rather than moving mid-scan.
- `upload` runs `rclone` without a shell (folder names are passed safely) and
  accepts `--remote NAME`.
- New modules: `cli`, `ui`, `context`.

### Safe moves (`feature/safe-move`)
- New shared `safeMove()` / `uniqueDestination()` helpers in `utils` replace
  three duplicated copies of the `_1`, `_2` collision-avoidance logic.
- **Fixed data loss in `sort`:** a file with the same name already in the
  target folder was silently overwritten. It is now renamed instead.
- `sort` now matches extensions case-insensitively (`.JPG`, `.PDF`, ...).
- Moves across volumes fall back to copy + remove instead of failing.
- `old` no longer overflows for very large day counts.

### Build foundation (`feature/build-foundation`)
- Rewrote the `Makefile`: it now builds every file in `src/` into `build/`,
  tracks header dependencies, and provides `install`, `uninstall` and `clean`.
- Added `.gitignore`; stopped tracking stale object files in `build/`.
- Added design documents in `docs/` for near-duplicate detection and the watch daemon.
- README now documents installation, commands and the project structure.
