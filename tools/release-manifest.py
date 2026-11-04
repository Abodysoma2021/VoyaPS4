"""Create the updater contract from the exact package being released."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
version = (root / "VERSION").read_text().strip()
if not re.fullmatch(r"\d+\.\d+\.\d+", version):
    raise SystemExit("VERSION must be a semantic version")
pkg = root / "dist/VoyaPS4.pkg"
digest = hashlib.sha256(pkg.read_bytes()).hexdigest()
manifest = {
    "schema": 1,
    "title_id": "VOYA00001",
    "version": version,
    "name": f"VoyaPS4 {version}",
    "description": "Native PS4 resumable download manager",
    "source": "https://github.com/Abodysoma2021/VoyaPS4",
    "url": f"https://github.com/Abodysoma2021/VoyaPS4/releases/download/v{version}/VoyaPS4.pkg",
    "size": pkg.stat().st_size,
    "sha256": digest,
}
(root / "dist/update.json").write_text(json.dumps(manifest, indent=2) + "\n")
(root / "dist/SHA256SUMS").write_text(f"{digest}  VoyaPS4.pkg\n")
print(f"VoyaPS4 {version}: {pkg.stat().st_size:,} bytes; SHA-256 {digest}")
