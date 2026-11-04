# Firmware 13.50 / GoldHEN acceptance checklist

Status: 0.1.0 installed on the user's PS4 but was rejected at launch with a corrupt-data
message and no error code. The target firmware was corrected from 13.52 to **13.50**.
Version 0.1.1 added the missing mandatory module stubs, then failed after controller
initialization with CE-34878-0. The first 0.1.2 diagnostic stayed on a black screen;
its boot log reported `Download folder is not a directory`. The linked SDK implements
`lstat` via an unsupported `fstatat` stub. The storage repair reached the interface.
Starting Apollo and reopening with that queued download then caused SIGSEGV at the
first call inside `transfer`, after reserving a 128 KiB stack buffer. The combined
repair moves streaming buffers to the heap; download acceptance remains pending.
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

If startup exits, read `/data/voyaps4/boot.log` over FTP. The latest-launch log records
entered_main, display_ready, font_ready, controller_ready, library_ready and first_frame,
or failed with the operation/error. A rejected package may never reach entered_main.
Kernel logging also records these milestones through GoldHEN's klog server. FTP
may need temporary read/traverse permissions on the boot log and its own directory;
do not change permissions on downloaded files or other console data.
Avoid sharing direct URLs containing private access tokens in public bug reports.
