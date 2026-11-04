# Changelog

## 0.1.2 — in development

### Fixed
- Replace unsupported PS4 `lstat`/`fstatat` with descriptor inspection and directory
  opens. The unsupported call prevented startup after controller initialization.
- Read local file lengths with 64-bit seek while retaining regular-file and symlink checks.
- Format kernel diagnostics before passing them to the platform's text export.
- Keep transfer, file hashing and update-copy buffers off small PS4 worker stacks.
  A console trace identified a stack fault at the first call inside `transfer`.

### Changed
- Send startup milestones and the original error directly to the console's kernel log,
  including failures that prevent creating the local boot log.
- Keep a failed startup alive for inspection instead of returning into musl `_exit`,
  which the firmware 13.50 crash trace identifies as a SIGSYS failure.

### Status
- The storage repair reached the interface on the console. Starting/resuming Apollo
  then exposed a separate worker-stack overflow; the combined repair awaits testing.

## 0.1.1 — 2026-11-04

### Fixed
- Include OpenOrbis's required libc.prx and libSceFios2.prx loader stubs; 0.1.0 omitted them.
- Check runtime completeness in both the GP4 manifest and the extracted final PKG.
- Reject failed or missing package-validation results even if the packaging CLI exits zero.

### Added
- Local latest-launch milestones and startup errors in `/data/voyaps4/boot.log`.

### Compatibility
- User clarified the actual target is firmware 13.50 with GoldHEN.
- Version 0.1.0 installed but was rejected at launch. Version 0.1.1 reached `main`
  but exited after the unsupported storage check; the console reported CE-34878-0.

## 0.1.0 — 2026-11-04

### Added
- Native PS4 controller interface and URL keyboard.
- Direct HTTP(S) package downloads and a configurable HTTPS catalog.
- Persistent queue, pause/resume, automatic reconnect retries and restart recovery.
- Range/validator checks, bounded streaming, size and optional SHA-256 verification.
- Native homebrew installation requests and staged app updates.
- Automatic GitHub releases when main contains a new VERSION.

### Known limitations
- Firmware 13.52 runtime behavior awaits physical-console testing.
- Keep the app open and console awake for downloads.
- App updates finish through the loader's installer after VoyaPS4 closes.
