# Third-party sources

VoyaPS4 is GPL-3.0-or-later; see LICENSE. SDK API references and packaging conventions
come from OpenOrbis. The compiled application links OpenOrbis's libc/libc++ and system
import stubs. The SDK supplies `right.sprx` during packaging; no Sony SDK is used.

- nlohmann/json 3.11.3, MIT: `vendor/json.hpp`, `vendor/json.LICENSE`.
  https://github.com/nlohmann/json/tree/v3.11.3
- stb_truetype, commit d14d46f6b44456eb88beac58cba833a4135d4c89, MIT/public domain:
  `vendor/stb_truetype.h`, `vendor/stb.LICENSE`.
  https://github.com/nothings/stb/tree/d14d46f6b44456eb88beac58cba833a4135d4c89
- Space Grotesk Medium, SIL Open Font License 1.1: `assets/fonts/SpaceGrotesk.ttf`,
  `assets/fonts/OFL.txt`. Static Medium face from the author's font repository:
  https://github.com/floriankarsten/space-grotesk/tree/03507d024a01282884232081fc6011c09ff4e849/fonts/ttf/static
- libcurl is used only by host tests, not linked into the PS4 package.

The Apollo package is downloaded from its author on demand; it is not redistributed
in VoyaPS4's package. Consult Apollo's own license and documentation.
