"""Exercise the real C++ transfer core against a fault-injecting HTTP server."""
import hashlib
import http.server
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / "build/test-driver"
PAYLOAD = b"\x7fCNT" + bytes(range(256)) * 8192
DIGEST = hashlib.sha256(PAYLOAD).hexdigest()
CHANGED = b"\x7fCNT" + bytes(reversed(range(256))) * 8192


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    requests = []
    dropped = set()
    failures = 0

    def log_message(self, *_):
        pass

    def handle(self):
        try:
            super().handle()
        except ConnectionResetError:
            pass

    def do_GET(self):
        requested = self.headers.get("Range", "")
        self.requests.append((self.path, requested, self.headers.get("If-Range")))
        if self.path == "/404":
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/500" or (self.path == "/flaky" and Handler.failures == 0):
            Handler.failures += 1
            self.send_response(500)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/range")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        offset = int(requested.split("=")[1].split("-")[0]) if requested else 0
        content = CHANGED if self.path == "/changed" and offset else PAYLOAD
        if self.path == "/not-pkg":
            content = b"<html>login required</html>"
        if self.path == "/large":
            if not offset:
                self.send_error(400)
                return
            self.send_response(206)
            self.send_header("Content-Range", f"bytes {offset}-{offset+len(content)-1}/{offset+len(content)}")
            self.send_header("Content-Length", str(len(content)))
            self.send_header("ETag", '"stable"')
            self.end_headers()
            self.wfile.write(content)
            return
        if offset >= len(content):
            self.send_response(416)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        etag = '"changed"' if content == CHANGED else '"stable"'
        partial = offset > 0 and self.path not in ("/ignore", "/changed")
        self.send_response(206 if partial else 200)
        if partial:
            start = offset + 1 if self.path == "/wrong-range" else offset
            self.send_header("Content-Range", f"bytes {start}-{len(content)-1}/{len(content)}")
        else:
            offset = 0
        self.send_header("Content-Length", str(len(content)-offset))
        self.send_header("ETag", etag)
        self.end_headers()
        try:
            if self.path == "/drop" and self.path not in self.dropped:
                self.dropped.add(self.path)
                self.wfile.write(content[:300000])
                self.wfile.flush()
                self.connection.shutdown(socket.SHUT_RDWR)
                self.connection.close()
                return
            self.wfile.write(content[offset:])
        except (BrokenPipeError, ConnectionResetError):
            pass  # A paused client intentionally closes its connection.


class Integration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.server.daemon_threads = True
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)

    def download(self, route="/range", pause=0, digest=DIGEST, expected=0, size=None):
        run = subprocess.run([str(DRIVER), "download", self.base + route,
                              str(len(PAYLOAD) if size is None else size), digest,
                              str(self.path), str(pause)], text=True, capture_output=True, timeout=20)
        self.assertEqual(run.returncode, expected, run.stderr + run.stdout)
        return run

    def completed(self, payload=PAYLOAD):
        files = list(self.path.glob("*.pkg"))
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0].read_bytes(), payload)

    def test_pause_then_resume_in_a_new_process(self):
        self.assertIn("paused", self.download(pause=200000).stdout)
        partial = next(self.path.glob("*.part"))
        saved = partial.stat().st_size
        self.assertGreater(saved, 0)
        self.download()
        self.completed()
        self.assertEqual(Handler.requests[-1][1:], (f"bytes={saved}-", '"stable"'))

    def test_network_disconnect_then_resume(self):
        self.download("/drop", expected=75)
        saved = next(self.path.glob("*.part")).stat().st_size
        self.assertGreater(saved, 0)
        self.download("/drop")
        self.completed()
        self.assertEqual(Handler.requests[-1][1], f"bytes={saved}-")

    def test_server_ignores_range_without_corrupting_file(self):
        self.download("/ignore", pause=200000)
        self.download("/ignore")
        self.completed()

    def test_wrong_content_range_never_appends(self):
        self.download("/wrong-range", pause=200000)
        partial = next(self.path.glob("*.part"))
        original = partial.read_bytes()
        self.assertIn("wrong byte range", self.download("/wrong-range", expected=1).stderr)
        self.assertEqual(partial.read_bytes(), original)

    def test_changed_resource_restarts_and_passes_new_hash(self):
        # No hash: If-Range prevents mixed versions; the whole new representation is saved.
        self.download("/changed", pause=200000, digest="")
        self.download("/changed", digest="")
        self.completed(CHANGED)

    def test_digest_mismatch_never_promotes(self):
        self.assertIn("SHA-256 mismatch", self.download(digest="0"*64, expected=1).stderr)
        self.assertFalse(list(self.path.glob("*.pkg")))

    def test_unknown_content_length_catalog_size_still_verifies(self):
        self.download(size=0)
        self.completed()

    def test_404_does_not_retry(self):
        self.assertIn("404", self.download("/404", expected=1).stderr)

    def test_500_is_retryable(self):
        self.assertIn("500", self.download("/500", expected=75).stderr)

    def test_redirect_to_relative_path(self):
        self.download("/redirect")
        self.completed()

    def test_rejects_size_conflict_before_writing(self):
        self.download(size=len(PAYLOAD)+1, expected=1)
        self.assertFalse(list(self.path.glob("*.part")))

    def test_rejects_non_package_file(self):
        self.assertIn("not a PS4 PKG", self.download("/not-pkg", size=0, digest="", expected=1).stderr)
        self.assertFalse(list(self.path.glob("*.pkg")))

    def test_real_queue_retries_without_user_action(self):
        Handler.failures = 0
        run = subprocess.run([str(DRIVER), "queue-download", self.base+"/flaky", str(len(PAYLOAD)),
                              DIGEST, str(self.path), "0"], capture_output=True, text=True, timeout=15)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertIn("attempts=1", run.stdout)
        self.completed()

    def test_more_than_4gb_offset_uses_64bit_range(self):
        # Sparse file: test 64-bit offsets without transferring or allocating 5 GB.
        url = self.base+"/large"
        offset = 5*1024*1024*1024
        total = offset + len(PAYLOAD)
        file_id = hashlib.sha256((url+"\n").encode()).hexdigest()[:24]
        part = self.path / (file_id+".pkg.part")
        with part.open("wb") as file:
            file.write(b"\x7fCNT")
            file.truncate(offset)
        (self.path/(file_id+".pkg.resume.json")).write_text(json.dumps({
            "url": url, "sha256": "", "size": total, "total": total, "validator": '"stable"'}))
        self.download("/large", size=total, digest="")
        self.assertEqual(Handler.requests[-1][1], f"bytes={offset}-")
        self.assertEqual(next(self.path.glob("*.pkg")).stat().st_size, total)

    def test_complete_partial_without_hash_restarts_after_416(self):
        self.download(pause=len(PAYLOAD), digest="")
        self.download(digest="")
        self.completed()

    def test_queue_recovery_and_paused_state(self):
        item = {"name": "test", "url": self.base+"/range", "size": len(PAYLOAD), "sha256": DIGEST}
        jobs = [{"item": item, "state": s} for s in ("queued", "paused", "cancelled", "failed")]
        (self.path/"queue.json").write_text(json.dumps({"schema": 1, "jobs": jobs}))
        run = subprocess.run([str(DRIVER), "recover", str(self.path)], text=True, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertTrue(run.stdout.startswith("Queued\nPaused\nCancelled\nNeeds attention\n"))

    def test_corrupt_queue_is_preserved(self):
        (self.path/"queue.json").write_text("{broken")
        run = subprocess.run([str(DRIVER), "recover", str(self.path)], text=True, capture_output=True)
        self.assertEqual(run.returncode, 0)
        self.assertEqual((self.path/"queue.recovery.json").read_text(), "{broken")

    def test_hash_padding_boundaries(self):
        for size in (0, 1, 55, 56, 63, 64, 65, 127, 128, 129, 100000):
            payload = bytes((n % 251 for n in range(size)))
            file = self.path/"hash-input"
            file.write_bytes(payload)
            run = subprocess.run([str(DRIVER), "hash", str(file)], text=True, capture_output=True)
            self.assertEqual(run.stdout.strip(), hashlib.sha256(payload).hexdigest())


if __name__ == "__main__":
    unittest.main(verbosity=2)
