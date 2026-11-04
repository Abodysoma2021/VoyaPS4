# VoyaPS4 0.1

Native PS4 homebrew download manager for direct URLs and user-controlled catalogs.
No commercial game catalog, DRM bypass, or proprietary SDK. Use OpenOrbis v0.5.4.

## Requirements

- Stream packages directly to PS4 storage with 64-bit lengths; keep bounded memory.
- Persist the queue and partial files under `/data/voyaps4`.
- Resume on relaunch and reconnect using Range plus If-Range. Validate Content-Range
  before appending. If the server ignores ranges, restart visibly without mixing files.
- Pause/resume per job, cancel without deleting data, exponential retry for transient
  failures, explicit errors for disk, permission, integrity and permanent HTTP errors.
- Validate size and optional SHA-256 before exposing an install action. Updater hashes
  are mandatory. Keep HTTPS verification on; never follow a TLS downgrade.
- Native 720p controller UI, on-screen URL keyboard, library and transfer views.
- Package installation requires a working homebrew loader. Request installation only
  after an explicit button press. Updating the running app uses the loader's package
  installer after closing VoyaPS4; queue state survives replacement.
- GitHub release pipeline: pushing a version tag builds and publishes a `.pkg`, hashes,
  and `update.json`. Ordinary pushes run tests and a PS4 package build.
- Include an original-author homebrew sample, linked to its source and pinned release.

## Structure and style

`src/` portable core, `src/platform/` HTTP/PS4 adapters, `tests/` behavioral tests,
`tools/` build/release utilities, `assets/` bundled font/catalog, `docs/` usage.
C++17, snake_case for functions, RAII ownership, explicit errors at IO boundaries.

```cpp
if (response.status == 206 && range.start != local_size)
    throw TransferError("Server returned the wrong byte range", false);
```

## Commands and validation

- `make test`: core tests and local HTTP integration (interruptions, ranges, restart,
  changed resources, errors, hash mismatch, queue recovery).
- `make preview`: render the real UI to a portable image.
- `make ps4 OO_PS4_TOOLCHAIN=/opt/OpenOrbis`: compile and package on Linux x86_64.
- `tools/docker-build.sh /absolute/path/to/OpenOrbis`: build on this Mac via Docker.
- Console checklist: `docs/CONSOLE-TESTING.md`; do not imply hardware testing occurred.

Always validate untrusted metadata and generated filenames; never publish secrets.
Changes to public release ownership or destructive removal of user packages require
separate authorization. Initial source and release publication are already authorized.

## Limits

Foreground downloads: keep VoyaPS4 open and PS4 awake. Rest-mode/background execution
is not promised. Retry after a temporary outage; resume after relaunch. Servers must
support byte ranges for efficient resume. Unknown source authenticity is shown clearly.
Native network/installer behavior on firmware 13.52 remains a hardware acceptance test.
