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
| `old [n]`          | Archive files untouched for `n`+ days into `OLD/` (default 30) |
| `upload <folder>`  | Upload a folder from Downloads to Google Drive via `rclone`    |

Run `filemgr` with no arguments to see the full usage.

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
│   ├── file_dedup.hpp
│   ├── file_old.hpp
│   ├── file_ops.hpp
│   ├── file_recent.hpp
│   ├── file_upload.hpp
│   └── utils.hpp         Shared helpers, managed-folder list, ThreadSafeQueue
└── src/                  Implementation, one file per command
    ├── main.cpp          Argument parsing and command dispatch
    ├── file_dedup.cpp    dedup: parallel SHA-256 duplicate detection
    ├── file_old.cpp      old: archive stale files into OLD/
    ├── file_ops.cpp      sort: move files into type folders
    ├── file_recent.cpp   recent: surface recently modified files
    ├── file_upload.cpp   upload: rclone wrapper
    └── utils.cpp         Hashing, paths, allowed-location check
```

Build output goes to `build/` (objects) and `./filemgr` (binary); both are git-ignored.
