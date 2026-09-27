# Changelog

All notable changes to filemgr. Each entry corresponds to a feature branch
merged into `main`.

## Unreleased

### Build foundation (`feature/build-foundation`)
- Rewrote the `Makefile`: it now builds every file in `src/` into `build/`,
  tracks header dependencies, and provides `install`, `uninstall` and `clean`.
- Added `.gitignore`; stopped tracking stale object files in `build/`.
- Added design documents in `docs/` for near-duplicate detection and the watch daemon.
- README now documents installation, commands and the project structure.
