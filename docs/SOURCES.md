# Verified implementation references

- OpenOrbis SDK v0.5.4: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.4
- Native network ABI: SDK `include/orbis/Http.h`, `Ssl.h`, `Net.h`, and
  `samples/net_http/net_http/main.c`. VoyaPS4 deliberately preserves TLS validation;
  it does not use the sample's certificate-bypass callback.
- Video, input, IME and installation ABI: SDK `VideoOut.h`, `Pad.h`, `UserService.h`,
  `ImeDialog.h`, `_types/ime_dialog.h`, `AppInstUtil.h`, `_types/sysmodule.h`.
- Byte ranges and If-Range semantics: https://www.rfc-editor.org/rfc/rfc9110#section-14
- Host libcurl streaming: https://curl.se/libcurl/c/libcurl-multi.html
- Resume behavior: https://everything.curl.dev/usingcurl/downloads/resume.html
- SHA-256: FIPS 180-4, https://csrc.nist.gov/pubs/fips/180-4/upd1/final
- Example homebrew author: https://github.com/bucanero/apollo-ps4/releases/tag/v2.4.0
  Its release API supplies the pinned package's size and SHA-256 digest.

OpenOrbis's two timeout/redirect exports have placeholder declarations in the SDK.
The native adapter provides typed symbol aliases; runtime ABI behavior is part of
the hardware checklist. No firmware support is inferred from compilation alone.
