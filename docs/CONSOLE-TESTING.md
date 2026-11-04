# Firmware 13.52 acceptance checklist

Status: **not run on a physical PS4**. Build success is not proof of runtime support.
Record the exact loader name/version, console model and network type when testing.

- Install VoyaPS4.pkg using an existing working homebrew package installer.
- Launch and confirm 720p safe-area layout, font, tab navigation, and controller reconnect.
- Add a URL with Triangle. Verify the native keyboard opens, accepts ASCII URLs and cancels.
- Download the bundled Apollo package; confirm expected size and SHA-256 success.
- Disconnect Wi-Fi/Ethernet mid-transfer; wait through a retry, reconnect, confirm resume.
- Pause, close the app normally, reopen, confirm paused stays paused and X resumes.
- Close during an active download, reopen, confirm recovery uses the real partial length.
- Test a server that ignores Range: full restart must be visible and the hash must pass.
- Test invalid certificates and HTTPS→HTTP redirects: downloads must fail; never disable TLS verification.
- Test low disk space, invalid package URL, 404 and a bad checksum; no install action should appear.
- Request installation of a small authorized homebrew app. Confirm it appears in PS4 notifications.
- Publish a higher VERSION, push main, wait for CI release. Confirm Updates detects it.
- Download/stage the update; close VoyaPS4 and use the loader's internal installer on
  `/data/pkg/VoyaPS4-update.pkg`. Relaunch and confirm new version and preserved queue.
- Keep the app open while downloading. Background/rest-mode transfers are outside this version.

If startup exits, record loader logs/stdout (VoyaPS4 prints the failing operation).
Avoid sharing direct URLs containing private access tokens in public bug reports.
