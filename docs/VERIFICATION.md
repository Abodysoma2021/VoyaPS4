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

The GitHub workflow repeats tests and package validation on the published source.
The live workflow/release page is the authority for its current result.

**Not verified:** physical PS4 launch, IME/controller behavior, native SceHttp TLS and
timeouts, native installation, and the full on-console update cycle on firmware 13.52.
Use [the hardware checklist](CONSOLE-TESTING.md) to establish compatibility.
