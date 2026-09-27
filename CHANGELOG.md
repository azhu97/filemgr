# Changelog

All notable changes to filemgr. Each entry corresponds to a feature branch
merged into `main`.

## Unreleased

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
