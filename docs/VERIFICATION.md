# Initial release verification — 2026-11-04

- `make test`: core SHA-256 vectors, URL/catalog/update policies, and 18 HTTP/queue
  integration tests pass on the development Mac.
- Fault cases include a dropped socket, pause/relaunch, an ignored range, incorrect
  Content-Range, a changed resource, a completed partial returning HTTP 416, size
  conflicts, invalid PKG bytes, bad hashes, HTTP 404/500 and automatic queue retries.
- A sparse-file test verifies an actual HTTP Range request starting beyond 5 GiB.
- SHA-256 is compared with Python hashlib at padding boundaries and large inputs.
- A deliberately inverted checksum comparison caused the integrity test to fail;
  the mutation was restored and the complete suite passed again.
- The host transfer adapter downloaded the original-author Apollo v2.4.0 package
  (22,806,528 bytes), matched its published SHA-256 and validated PKG magic.
- `make preview`: all four screens rendered from native UI code; Library, Downloads
  and Updates were visually inspected. Preview download progress is synthetic.
- `make ps4` in the isolated Linux x86_64 Docker builder: compiled native application,
  produced PKG, passed every `PkgTool.Core pkg_validate` digest/signature check.
- Vendored dependency checksums, shell syntax, workflow YAML and staged whitespace checked.

GitHub rejected the initial workflow dispatch with HTTP 422: "Actions has been
disabled for this user." Repository Actions permissions are enabled, so this is an
account restriction. The initial release uses the verified local builder and release
script. The committed workflow can repeat these checks when Actions is available.

Published release: https://github.com/Abodysoma2021/VoyaPS4/releases/tag/v0.1.0
The public `latest/download/VoyaPS4.pkg` URL was downloaded again through the host
transfer engine and matched the published updater manifest: 6,619,136 bytes,
SHA-256 `c50698573ba0b668188a4c9befefa9abe484edf74b19959a3ae63402933b111d`.

**Not verified:** physical PS4 launch, IME/controller behavior, native SceHttp TLS and
timeouts, native installation, and the full on-console update cycle on firmware 13.52.
Use [the hardware checklist](CONSOLE-TESTING.md) to establish compatibility.

## 0.1.1 launch repair — 2026-11-04

The user corrected the console firmware to **13.50 / GoldHEN**. Version 0.1.0 was
installed through GoldHEN from the FTP-uploaded PKG, but launching it produced a
corrupt-data/reinstall message without an error code. The installed title exists.

Inspection found both mandatory OpenOrbis stubs absent: `sce_module/libc.prx` and
`sce_module/libSceFios2.prx`. The new validator reproduced this defect against the
original GP4 and the extracted original package. OpenOrbis's own `src/README.md`
documents that these modules are required in packages.

The corrected native build includes the SDK stubs. The build now validates the GP4,
builds and checks PKG digests/signatures, extracts the actual final PKG, extracts its
PARAM_SFO and ICON0 SC entries, and validates the resulting complete runtime layout.
Both pre-package and post-extraction checks passed for the 0.1.1 candidate.

All 18 transfer/queue integration tests, core assertions and six new package-layout
tests pass. Startup milestones are recorded in `/data/voyaps4/boot.log` if the console
loader reaches `main`. On-console launch and the remaining runtime tests are pending.
