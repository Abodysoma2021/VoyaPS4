Version 0.1.1 fixes a packaging defect in 0.1.0: the mandatory OpenOrbis libc.prx and
libSceFios2.prx stubs were missing. The build now extracts its final PKG and checks the
runtime files in addition to digest/signature validation. Startup milestones are saved
locally in `/data/voyaps4/boot.log` for console debugging.

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

The user's actual target is firmware 13.50 with GoldHEN; the earlier 13.52 report was
corrected. The prior version installed but failed to launch. This fix still needs a
successful on-console launch and runtime tests. Host tests and native package checks
run locally because GitHub Actions is disabled for the account. Read the
console checklist before reporting compatibility.

The bundled example points to Apollo Save Tool's original-author GitHub release.
No commercial game downloads, proprietary SDK, exploits or DRM bypass are included.
