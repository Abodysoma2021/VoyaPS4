"""Check runtime completeness, including files that PKG digest validation cannot detect."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

REQUIRED_FILES = (
    "eboot.bin",
    "sce_module/libc.prx",
    "sce_module/libSceFios2.prx",
    "sce_sys/param.sfo",
    "sce_sys/icon0.png",
    "sce_sys/about/right.sprx",
    "assets/catalog.json",
    "assets/fonts/SpaceGrotesk.ttf",
    "assets/fonts/OFL.txt",
    "assets/LICENSE.txt",
    "assets/THIRD-PARTY.txt",
    "assets/json.LICENSE",
    "assets/stb.LICENSE",
)
SELF_FILES = ("eboot.bin", "sce_module/libc.prx", "sce_module/libSceFios2.prx", "sce_sys/about/right.sprx")


def validate(root, gp4=None, validation_log=None):
    root = Path(root)
    if gp4 is not None:
        entries = {entry.attrib["targ_path"] for entry in ET.parse(gp4).findall("./files/file")}
        missing = set(REQUIRED_FILES) - entries
        if missing:
            raise ValueError("Package manifest omits required files: " + ", ".join(sorted(missing)))
    for relative in REQUIRED_FILES:
        file = root / relative
        if file.is_symlink() or not file.is_file() or file.stat().st_size == 0:
            raise ValueError("Missing, empty or invalid package file: " + relative)
    for relative in SELF_FILES:
        with (root / relative).open("rb") as file:
            if file.read(4) != bytes.fromhex("4f153d1d"):
                raise ValueError("Invalid SELF header: " + relative)
    with (root / "sce_sys/param.sfo").open("rb") as file:
        if file.read(4) != b"\0PSF":
            raise ValueError("Invalid parameter file")
    with (root / "sce_sys/icon0.png").open("rb") as file:
        if file.read(8) != b"\x89PNG\r\n\x1a\n":
            raise ValueError("Invalid package icon")
    if validation_log is not None:
        lines = Path(validation_log).read_text().splitlines()
        results = [line for line in lines if line.startswith("[")]
        if not results or any(not line.startswith("[OK]") for line in results):
            raise ValueError("PKG digest/signature validation did not pass")
        if not any("(Signature) PKG Header Signature" in line for line in results):
            raise ValueError("PKG header signature result is missing")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--gp4", type=Path)
    parser.add_argument("--validation-log", type=Path)
    args = parser.parse_args()
    try:
        validate(args.root, args.gp4, args.validation_log)
    except (ValueError, OSError, ET.ParseError) as error:
        raise SystemExit(str(error))
    print("Package runtime layout verified (including libc.prx and libSceFios2.prx)")
