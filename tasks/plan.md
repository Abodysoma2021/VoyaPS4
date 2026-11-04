# Implementation plan

1. Build a portable transfer engine and fault-injecting HTTP tests first.
2. Add durable queue, catalogs, metadata validation, update version policy.
3. Connect PS4 HTTP, video, controller, URL keyboard and installer adapters.
4. Render/review the native UI, build an OpenOrbis PKG, run host integration tests.
5. Publish source and versioned release to GitHub; verify downloadable artifacts.

Use HTTPS plus release SHA-256 for transport/integrity. This is not an independent
signature system: a compromised release account can publish a malicious release.
Never disable certificate validation to compensate for old console certificates.
