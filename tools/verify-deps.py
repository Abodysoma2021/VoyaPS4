import hashlib
from pathlib import Path

root = Path(__file__).resolve().parents[1]
expected = {
    "vendor/json.hpp": "9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6",
    "vendor/stb_truetype.h": "ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab",
    "assets/fonts/SpaceGrotesk.ttf": "3183b7eb0b4241476360e2cd8e527868616cc16d3ce332c4376505989b772b44",
}
for path, digest in expected.items():
    if hashlib.sha256((root / path).read_bytes()).hexdigest() != digest:
        raise SystemExit(f"Dependency checksum mismatch: {path}")
print(f"Verified {len(expected)} vendored dependency checksums")
