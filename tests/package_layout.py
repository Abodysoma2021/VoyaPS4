import runpy
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
validator = runpy.run_path(str(ROOT / "tools/validate-package.py"))
validate = validator["validate"]
required = validator["REQUIRED_FILES"]


class PackageLayout(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in required:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"content")
        for name in validator["SELF_FILES"]:
            (self.root / name).write_bytes(bytes.fromhex("4f153d1d") + b"fixture")
        (self.root / "sce_sys/param.sfo").write_bytes(b"\0PSFfixture")
        (self.root / "sce_sys/icon0.png").write_bytes(b"\x89PNG\r\n\x1a\nfixture")

    def test_complete_layout_is_accepted(self):
        validate(self.root)

    def test_missing_runtime_is_rejected(self):
        for name in ("sce_module/libc.prx", "sce_module/libSceFios2.prx"):
            path = self.root / name
            original = path.read_bytes()
            path.unlink()
            with self.assertRaisesRegex(ValueError, name):
                validate(self.root)
            path.write_bytes(original)

    def test_manifest_omission_is_rejected_even_if_staged_file_exists(self):
        project = ET.Element("psproject")
        files = ET.SubElement(project, "files")
        for name in required:
            if name != "sce_module/libc.prx":
                ET.SubElement(files, "file", targ_path=name, orig_path=name)
        manifest = self.root / "package.gp4"
        ET.ElementTree(project).write(manifest)
        with self.assertRaisesRegex(ValueError, "manifest omits.*libc.prx"):
            validate(self.root, manifest)

    def test_invalid_runtime_header_is_rejected(self):
        (self.root / "sce_module/libc.prx").write_bytes(b"not a SELF")
        with self.assertRaisesRegex(ValueError, "Invalid SELF"):
            validate(self.root)

    def test_failed_pkg_validation_is_rejected_even_if_cli_exits_successfully(self):
        log = self.root / "validation.log"
        log.write_text("[OK] (Signature) PKG Header Signature\n[FAIL] (Hash) Body Digest\n")
        with self.assertRaisesRegex(ValueError, "did not pass"):
            validate(self.root, validation_log=log)

    def test_successful_pkg_validation_is_accepted(self):
        log = self.root / "validation.log"
        log.write_text("[OK] (Signature) PKG Header Signature\n[OK] (Hash) Body Digest\n")
        validate(self.root, validation_log=log)


if __name__ == "__main__":
    unittest.main(verbosity=2)
