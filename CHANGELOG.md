# Changelog

All notable changes to filemgr. Each entry corresponds to a feature branch
merged into `main`.

## Unreleased

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
