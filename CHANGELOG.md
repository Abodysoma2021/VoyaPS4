# Changelog

## 0.1.2 — in development

### Changed
- Send startup milestones and the original error directly to the console's kernel log,
  including failures that prevent creating the local boot log.
- Keep a failed startup alive for inspection instead of returning into musl `_exit`,
  which the firmware 13.50 crash trace identifies as a SIGSYS failure.

### Status
- Diagnostic candidate; the original startup error is still being isolated on the console.

## 0.1.1 — 2026-11-04

### Fixed
- Include OpenOrbis's required libc.prx and libSceFios2.prx loader stubs; 0.1.0 omitted them.
- Check runtime completeness in both the GP4 manifest and the extracted final PKG.
- Reject failed or missing package-validation results even if the packaging CLI exits zero.

### Added
- Local latest-launch milestones and startup errors in `/data/voyaps4/boot.log`.

### Compatibility
- User clarified the actual target is firmware 13.50 with GoldHEN.
- Version 0.1.0 installed but was rejected at launch. Version 0.1.1 requires on-console verification.

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
