# filemgr

A macOS command-line tool that keeps your `~/Downloads` folder under control:
sort files by type, remove duplicates, archive stale files, surface recent
files, and upload folders to Google Drive.

## Install

```sh
make                 # builds ./filemgr
make install         # binary, man page and shell completions under ~/.local (override with PREFIX=...)
make test            # unit + end-to-end tests (never touch your real Downloads)
```

Make sure `~/.local/bin` is on your `PATH`, or install system-wide with
`sudo make install PREFIX=/usr/local` (Homebrew's zsh, bash-completion and fish
pick up completions from there automatically).

With the default `~/.local` prefix, point your shell at the installed completions:

```sh
# zsh (~/.zshrc, before compinit)
fpath=(~/.local/share/zsh/site-functions $fpath)
# bash (~/.bashrc)
source ~/.local/share/bash-completion/completions/filemgr
# fish: ~/.local/share/fish/vendor_completions.d is not searched by default
filemgr completions fish > ~/.config/fish/completions/filemgr.fish
```

`man filemgr` works once `~/.local/share/man` is on your `MANPATH` (or use
`man ./man/filemgr.1` from the repo).

Requires macOS and the Xcode command line tools (`xcode-select --install`).

## Commands

| Command            | What it does                                                   |
|--------------------|----------------------------------------------------------------|
| `sort`             | Move top-level files into type folders (`IMAGES`, `DOCUMENTS`, ...) |
| `recent [n]`       | Bring the `n` most recently modified files back to the top level (default 5) |
| `dedup [--near]`   | Move byte-identical duplicates into `DUPLICATES/`, keeping the oldest copy. `--near` also moves visually similar images (resized, re-compressed, converted) into `DUPLICATES/NEAR/`; tune with `--threshold N` |
| `old [days]`       | Archive files untouched for `days`+ days into `OLD/` (default 30) |
| `upload <folder>`  | Upload a folder to Google Drive via `rclone` (`--remote NAME` to pick a remote) |
| `clean [rule...]`  | Apply your cleanup rules from the config (`--list` to preview match counts) |
| `stats`            | Disk usage by type, folder and age; largest files; duplicate waste and other cleanup opportunities. `--html FILE` writes a standalone report page |
| `find [pattern]`   | Search by name, `--type`, `--ext`, `--larger`/`--smaller` size, `--older`/`--newer` age (read-only) |
| `watch`            | Sort new files automatically as they land (runs until Ctrl-C; see below) |
| `history [id]`     | List recent runs, or every move made by run `id` (`--limit N`) |
| `undo [id]`        | Reverse the last run (or run `id`), moving files back where they came from |
| `config [action]`  | `show` the effective config, print its `path`, `init` a starter file, or `edit` it |
| `completions <shell>` | Print a completion script for `zsh`, `bash` or `fish` |
| `help [command]`   | Show usage, or detailed help for one command                   |

### Global options

| Option              | Effect                                                    |
|---------------------|-----------------------------------------------------------|
| `-n`, `--dry-run`   | Show what would happen without changing anything          |
| `-p`, `--path DIR`  | Manage `DIR` instead of `~/Downloads`                     |
| `-c`, `--config FILE` | Use a different config file                             |
| `-v`, `--verbose`   | Extra detail (skipped files, thread counts, timing)       |
| `-q`, `--quiet`     | Only errors and the final summary line                    |
| `--no-color`        | Plain output (also honoured via `$NO_COLOR`)              |
| `--time`            | Print how long the command took                           |
| `--no-journal`      | Don't record this run's moves (it can't be undone)        |
| `-h`, `--help`      | Help; `filemgr <command> --help` for a single command     |
| `--version`         | Print the version                                         |

Options can go before or after the command: `filemgr -n sort` and
`filemgr sort --dry-run` are equivalent. Setting `$FILEMGR_ROOT` changes the
default folder, which is handy for experimenting on a scratch directory.

Exit status is `0` on success, `1` on a runtime error, and `2` on a usage error.

## Configuration

filemgr works with no configuration. To customise it, run `filemgr config init`
to write a commented starter file to `~/.config/filemgr/config` (or
`$XDG_CONFIG_HOME/filemgr/config`, or `$FILEMGR_CONFIG`), then `filemgr config edit`.

```ini
[general]
root = ~/Downloads        # folder to manage
remote = gdrive           # rclone remote for upload
old_days = 30             # default for 'filemgr old'
recent_count = 5          # default for 'filemgr recent'
threads = 0               # dedup hashing threads (0 = one per core)

[categories]
SPREADSHEETS = csv xlsx numbers   # new folder; takes these extensions from DOCUMENTS
IMAGES = jpg png heic             # redefining a built-in folder replaces its list
```

Custom category folders are managed exactly like the built-in ones. The folder
to manage is chosen in this order: `--path`, `$FILEMGR_ROOT`, `root` in the
config, then `~/Downloads`. Run `filemgr config` to see the result.

## Cleanup rules

Rules automate routine cleanup. Add them to your config (`filemgr config edit`):

```ini
[rule "Old installers"]
ext = dmg pkg
older = 14d
action = trash

[rule "Stale duplicates"]
in = DUPLICATES
older = 30d
action = trash

[rule "Screenshots"]
match = Screenshot*.png "Screen Shot*"
action = move SCREENSHOTS
```

| Key       | Meaning                                                        |
|-----------|----------------------------------------------------------------|
| `match`   | Filename globs (case-insensitive; quote patterns with spaces)  |
| `ext`     | Extensions, e.g. `dmg pkg`                                     |
| `type`    | Category folder, e.g. `INSTALLERS` (or `Other`)                |
| `larger` / `smaller` | Size bounds, e.g. `100M`                            |
| `older` / `newer`    | Last-modified bounds, e.g. `14d`, `6m`, `1y`         |
| `in`      | Only files in this top-level folder (`.` = the top level)      |
| `action`  | `trash` or `move FOLDER`                                       |
| `enabled` | `false` to turn a rule off (you can still run it by name)     |

```sh
filemgr clean --list        # rules and how many files each matches right now
filemgr clean -n            # preview
filemgr clean               # run all enabled rules
filemgr clean "Old installers"
filemgr undo                # changed your mind? trashed files come back too
```

Rules only see files filemgr manages: the top level, type folders and
`DUPLICATES`. Your own folders are never touched. Each file gets the first rule
it matches. A rule must have at least one condition, so a typo can't trash
everything. Nothing is ever deleted: `trash` moves files to `~/.Trash`. A
`move` target that isn't a category folder becomes one of your own folders, so
filemgr leaves those files alone from then on.

## Undo

Every command that moves files records each move in `~/.filemgr/journal`
(override with `$FILEMGR_STATE_DIR`). Nothing is ever deleted, so any run can
be reversed:

```sh
filemgr history          # #12 2026-09-27 10:02  dedup  4 moves
filemgr history 12       # every move made by run #12
filemgr undo             # reverse the most recent run
filemgr undo 12          # reverse a specific run
```

`undo` is journaled as well, so undoing an undo re-applies the original run.
If a file has since been moved or deleted it is skipped with a warning, and if
its original name has been taken the file is restored as `name_1.ext`.

## Reports with `stats`

`filemgr stats` is read-only. It shows where the space goes and which filemgr
commands would reclaim it:

```
By type
  VIDEOS       ████████████████████████     1.5 GB  47%  3 files
  INSTALLERS   ████████████████████▊        1.3 GB  40%  2 files
  ...
Cleanup opportunities
  33 duplicate copies wasting 137.1 MB   filemgr dedup
  14 files untouched for 30+ days (2.3 GB)   filemgr old
  4 unsorted files at the top level   filemgr sort
```

`filemgr stats --html report.html` writes the same report as a single
self-contained HTML page with charts, tables and dark mode. `--top N` sets how
many large files to list, and `--no-dups` skips the duplicate scan on very
large folders.

## Searching with `find`

```sh
filemgr find invoice                       # name contains "invoice" (case-insensitive)
filemgr find "IMG_*.heic" --older 6m       # glob + last modified over 6 months ago
filemgr find --type VIDEOS --larger 500M --sort size
filemgr find --ext dmg,pkg --in INSTALLERS
filemgr find --older 1y --print0 | xargs -0 open -R    # reveal matches in Finder
```

Sizes take `K`/`M`/`G`/`T` suffixes. Ages take `h`/`d`/`w`/`m`/`y`, and a bare
number means days. `find` searches everything under the managed folder,
including your own folders, because it never changes anything. It exits with
status 1 when nothing matches.

## Automatic sorting with `watch`

`filemgr watch` stays running and sorts each new file into its type folder as
soon as the download finishes. It waits for the file to stop changing and
ignores in-progress `.crdownload`/`.part` files.

```sh
filemgr watch                    # foreground, Ctrl-C to stop
filemgr watch --settle 5         # wait 5s of quiet before sorting
filemgr watch --sort-existing    # also sort what's already there at startup
```

To run it at login as a launchd agent:

```sh
filemgr watch --print-plist > ~/Library/LaunchAgents/com.filemgr.watch.plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/com.filemgr.watch.plist
tail -f ~/Library/Logs/filemgr-watch.log
```

To stop and remove it:

```sh
launchctl bootout gui/$(id -u)/com.filemgr.watch
rm ~/Library/LaunchAgents/com.filemgr.watch.plist
```

Only one watcher can run at a time. macOS may ask for permission to access
Downloads the first time the agent runs. A hand-editable template lives in
`launchd/com.filemgr.watch.plist`.

## Protected folders

filemgr only ever touches files sitting directly in Downloads or inside the
folders it manages itself. Any folder you create (for example `PROTECTED`) is
invisible to it and never manipulated — put files there to keep them safe.

## Google Drive upload

1. `brew install rclone`
2. `rclone config` — create a Google Drive remote named `gdrive`.
3. `filemgr upload <folder>`

## Project structure

```
filemgr/
├── .github/workflows/
│   └── ci.yml            CI on macOS: -Werror build, tests, man lint, install
├── Makefile              Build, test, install (binary, man page, completions)
├── README.md             This file
├── CHANGELOG.md          Notable changes per feature
├── docs/                 Design documents for larger features
│   ├── near-duplicate-detection.md
│   └── watch-daemon.md
├── launchd/
│   └── com.filemgr.watch.plist  launchd agent template for `watch`
├── man/
│   └── filemgr.1         Man page
├── include/              Public headers, one per module
│   ├── cli.hpp           Argument parser (options, positionals, help formatting)
│   ├── commands.hpp      Command table: names, options, handlers (drives help + completions)
│   ├── completions.hpp   Shell completion generator
│   ├── config.hpp        Config file format, defaults, rules and loader
│   ├── context.hpp       Context: managed root, config, journal, run-wide flags
│   ├── duplicates.hpp    Exact-duplicate search shared by dedup and stats
│   ├── file_clean.hpp    clean command (rules engine)
│   ├── file_config.hpp   config command
│   ├── file_dedup.hpp
│   ├── file_find.hpp     find command options
│   ├── file_history.hpp  history / undo commands
│   ├── file_near.hpp     dedup --near pass and its thresholds
│   ├── file_old.hpp
│   ├── file_ops.hpp
│   ├── file_recent.hpp
│   ├── file_upload.hpp
│   ├── file_watch.hpp    watch command and launchd plist generation
│   ├── filter.hpp        FileFilter + size/age parsing (shared by find and rules)
│   ├── image_hash.hpp    Perceptual image fingerprints (dHash + color grid)
│   ├── journal.hpp       Append-only move journal (runs, moves, undo markers)
│   ├── stats.hpp         Folder statistics model and stats/HTML entry points
│   ├── ui.hpp            Colored, leveled, thread-safe terminal output
│   └── utils.hpp         Shared helpers: safeMove, hashing, allowed-location check
├── src/                  Implementation, one file per module
│   ├── cli.cpp
│   ├── commands.cpp      Every subcommand registered in one table
│   ├── completions.cpp   zsh/bash/fish script generation
│   ├── config.cpp        Built-in categories and INI parser
│   ├── duplicates.cpp    Size bucketing + parallel SHA-256 grouping
│   ├── file_clean.cpp    Rule planning (first match wins) and trash/move actions
│   ├── file_config.cpp   config show/path/init/edit
│   ├── main.cpp          Context setup (root, config, journal) and dispatch
│   ├── file_dedup.cpp    dedup: keeps oldest copy of each duplicate group
│   ├── file_find.cpp     find: filtered, sorted listing
│   ├── file_history.cpp  history listing and undo
│   ├── file_near.cpp     Near-duplicate grouping, keeps highest-resolution copy
│   ├── file_old.cpp      old: archive stale files into OLD/
│   ├── file_ops.cpp      sort: move files into type folders (sortOneFile shared with watch)
│   ├── file_recent.cpp   recent: surface recently modified files
│   ├── file_upload.cpp   upload: rclone wrapper (spawned without a shell)
│   ├── file_watch.cpp    FSEvents stream, settle/debounce logic, lock file
│   ├── filter.cpp
│   ├── image_hash.cpp    ImageIO decoding + hashing
│   ├── journal.cpp
│   ├── report_html.cpp   Self-contained HTML report renderer
│   ├── stats.cpp         stats collection and terminal rendering
│   ├── ui.cpp
│   └── utils.cpp
└── tests/
    ├── testing.hpp       Tiny test framework (TEST, CHECK, CHECK_EQ, CHECK_THROWS)
    ├── test_main.cpp     Runner: temp dir per test, isolated state/config
    ├── test_cli.cpp      Argument parser
    ├── test_config.cpp   Config loading and validation
    ├── test_utils.cpp    safeMove, journal, hashing, allowed locations
    ├── test_commands.cpp sort/dedup/old/recent/undo on fake folders
    ├── test_filter.cpp   Size/age parsing and filter matching
    ├── test_near.cpp     Perceptual hashing on generated images
    ├── test_rules.cpp    Rule parsing/validation and clean + undo
    ├── test_stats.cpp    Stats collection and HTML escaping
    └── e2e.sh            End-to-end tests of the built binary (incl. completions, docs coverage)
```

Build output goes to `build/` (objects) and `./filemgr` (binary); both are git-ignored.

### Adding a command

1. Add `include/file_<name>.hpp` declaring `int <name>(const Context& ctx, ...)`.
2. Implement it in `src/file_<name>.cpp`. Move files only through `safeMove()`
   so dry runs (and the undo journal) work automatically, and gate any recursive
   scan with `isInAllowedLocation()` so user folders stay untouched.
3. Register it in the command table in `src/commands.cpp`, with `journaled = true`
   if it moves files. Help text and shell completions pick it up automatically.
   Document it in `man/filemgr.1` and this README, because `make test` fails
   if either is missing a command.
4. Add tests in `tests/test_commands.cpp` (and `tests/e2e.sh` for CLI
   behaviour), then run `make test`. Run a subset with `build/unit_tests <name-filter>`.
