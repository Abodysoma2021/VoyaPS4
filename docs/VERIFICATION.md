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

## 0.1.2 storage startup repair — 2026-11-04

The 0.1.1 console crash trace showed SIGSYS in libkernel `_exit`, reached after
`main` returned 1. Keeping a diagnostic failure alive exposed the original error:
`Download folder is not a directory`, after display/font/controller initialization.
The user observed a fixed black screen in that diagnostic.

Disassembly of the actual linked OpenOrbis 0.5.4 executable proves `lstat` calls
`fstatat`, which only stores errno 78 (ENOSYS) and returns -1. The upstream
[PS4 implementation](https://github.com/OpenOrbis/musl/blob/master/src/stat/fstatat.c)
also explicitly returns ENOSYS. This was an unsupported API, not a missing directory.

The storage regression test forces every `lstat` call to return ENOSYS. Before the
repair it failed with the exact console message. After replacing those calls with
`open`/`fstat`/`lseek` and directory opens, it passes, including symlink, FIFO,
non-directory, missing-file and sparse-file lengths above 5 GiB. All 18 HTTP/queue
tests, core checks and six package-layout tests also pass on the development Mac.
Kernel messages are now preformatted, matching the SDK trophies sample.

The repaired native candidate compiled and its package digest/signature and extracted
runtime-layout checks passed. Symbol inspection confirms neither `lstat` nor
`fstatat` remains linked; the supported descriptor `fstat` is linked instead.

The console reached `first_frame` and the user confirmed the interface appeared.
Starting Apollo then crashed; reopening with its saved queued job reproduced it.
The full trace records SIGSEGV at `0x423dbe`, a `call validate_item` immediately after
`transfer` reserves 0x20258 bytes of stack. The fault address is exactly `rsp - 8`,
the return-address write into unmapped stack memory. This establishes a separate
worker-stack overflow before network I/O in the transfer.

A regression downloads and hashes a 256 KiB synthetic PKG on a pthread explicitly
limited to 64 KiB. Before the buffer repair it terminated with SIGBUS on the Mac.
The transfer, file-hash and update-copy buffers now use bounded 128 KiB heap storage.
The 64 KiB test and all existing host checks pass after the repair. The combined
native candidate builds and passes package digest/signature and extracted-layout checks.
Disassembly confirms the transfer and file-hash stack reservations are below 16 KiB.
Console download, native installation and update acceptance remain pending.
