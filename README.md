# filemgr

A macOS command-line tool that keeps your `~/Downloads` folder under control:
sort files by type, remove duplicates, archive stale files, surface recent
files, and upload folders to Google Drive.

## Install

```sh
make                 # builds ./filemgr
make install         # installs to ~/.local/bin/filemgr (override with PREFIX=...)
```

Make sure `~/.local/bin` is on your `PATH`, or install system-wide with
`sudo make install PREFIX=/usr/local`.

Requires macOS and the Xcode command line tools (`xcode-select --install`).

## Commands

| Command            | What it does                                                   |
|--------------------|----------------------------------------------------------------|
| `sort`             | Move top-level files into type folders (`IMAGES`, `DOCUMENTS`, ...) |
| `recent [n]`       | Bring the `n` most recently modified files back to the top level (default 5) |
| `dedup`            | Move byte-identical duplicates into `DUPLICATES/`, keeping the oldest copy |
| `old [days]`       | Archive files untouched for `days`+ days into `OLD/` (default 30) |
| `upload <folder>`  | Upload a folder to Google Drive via `rclone` (`--remote NAME` to pick a remote) |
| `help [command]`   | Show usage, or detailed help for one command                   |

### Global options

| Option              | Effect                                                    |
|---------------------|-----------------------------------------------------------|
| `-n`, `--dry-run`   | Show what would happen without changing anything          |
| `-p`, `--path DIR`  | Manage `DIR` instead of `~/Downloads`                     |
| `-v`, `--verbose`   | Extra detail (skipped files, thread counts, timing)       |
| `-q`, `--quiet`     | Only errors and the final summary line                    |
| `--no-color`        | Plain output (also honoured via `$NO_COLOR`)              |
| `--time`            | Print how long the command took                           |
| `-h`, `--help`      | Help; `filemgr <command> --help` for a single command     |
| `--version`         | Print the version                                         |

Options can go before or after the command: `filemgr -n sort` and
`filemgr sort --dry-run` are equivalent. Setting `$FILEMGR_ROOT` changes the
default folder, which is handy for experimenting on a scratch directory.

Exit status is `0` on success, `1` on a runtime error, and `2` on a usage error.

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
├── Makefile              Build, install and clean targets
├── README.md             This file
├── CHANGELOG.md          Notable changes per feature
├── docs/                 Design documents for larger features
│   ├── near-duplicate-detection.md
│   └── watch-daemon.md
├── include/              Public headers, one per module
│   ├── cli.hpp           Argument parser (options, positionals, help formatting)
│   ├── context.hpp       Context: managed root folder and run-wide flags
│   ├── file_dedup.hpp
│   ├── file_old.hpp
│   ├── file_ops.hpp
│   ├── file_recent.hpp
│   ├── file_upload.hpp
│   ├── ui.hpp            Colored, leveled, thread-safe terminal output
│   └── utils.hpp         Shared helpers: safeMove, hashing, allowed-location check
└── src/                  Implementation, one file per module
    ├── cli.cpp
    ├── main.cpp          Command table, global options, dispatch
    ├── file_dedup.cpp    dedup: size pre-filter + parallel SHA-256
    ├── file_old.cpp      old: archive stale files into OLD/
    ├── file_ops.cpp      sort: move files into type folders
    ├── file_recent.cpp   recent: surface recently modified files
    ├── file_upload.cpp   upload: rclone wrapper (spawned without a shell)
    ├── ui.cpp
    └── utils.cpp
```

Build output goes to `build/` (objects) and `./filemgr` (binary); both are git-ignored.

### Adding a command

1. Add `include/file_<name>.hpp` declaring `int <name>(const Context& ctx, ...)`.
2. Implement it in `src/file_<name>.cpp`. Move files only through `safeMove()`
   so dry runs (and the undo journal) work automatically, and gate any recursive
   scan with `isInAllowedLocation()` so user folders stay untouched.
3. Register it in the command table in `src/main.cpp`.
