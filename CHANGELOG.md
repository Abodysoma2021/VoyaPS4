# Changelog

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
