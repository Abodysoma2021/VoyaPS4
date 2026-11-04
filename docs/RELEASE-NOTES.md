Native PS4 homebrew download manager with a persistent queue, pause/resume,
reconnect retries, byte-range validation, SHA-256 verification and update discovery.

Download **VoyaPS4.pkg** and install it with your existing homebrew package installer.
Initial installation is also possible through an existing URL-capable remote package
installer using the direct release asset URL. A normal PS4 browser cannot install a
PKG merely by visiting a webpage.

Keep VoyaPS4 open and the console awake while downloading. On relaunch, queued and
interrupted transfers resume; paused/cancelled jobs remain stopped. App updates are
verified and staged in `/data/pkg/VoyaPS4-update.pkg` for installation after closing
VoyaPS4. A working loader with internal package installation is required for this path.

This is an initial hardware-unverified release. Host fault-injection tests and
OpenOrbis package validation run in CI. Firmware 13.52 runtime, controller keyboard,
TLS and installation behavior still need testing on a real console. Read the
console checklist before reporting compatibility.

The bundled example points to Apollo Save Tool's original-author GitHub release.
No commercial game downloads, proprietary SDK, exploits or DRM bypass are included.
