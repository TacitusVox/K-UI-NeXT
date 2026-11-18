#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run production HTTP, TLS, and BBA socket code against loopback TLS origins."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading

from test_media_tls import create_test_ca, HOST

ROOT = Path(__file__).resolve().parents[1]
DATA = bytes(range(256)) * 39
PREFIX = b"HTTP/1.1 200 OK\r\n"
LENGTH = b"Content-Length: " + str(len(DATA)).encode() + b"\r\n\r\n"
OK, MISSING, LIMITED, STOPPED, ERROR, LARGE = range(6)


class Origin(BaseHTTPRequestHandler):
    counts = {}

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path in ("/prefix-length", "/prefix-chunk", "/prefix-error"):
            # Advertise much more data than we send; an accepted prefix must
            # finish before an unclean EOF or an incomplete chunk terminator.
            framing = (b"Transfer-Encoding: chunked\r\n\r\n20000\r\n"
                       if self.path == "/prefix-chunk" else b"Content-Length: 131072\r\n\r\n")
            self.wfile.write(PREFIX + framing + DATA[:1500])
            return
        assert self.headers["Host"] == HOST
        assert self.headers["Accept-Encoding"] == "identity"
        self.counts[self.path] = self.counts.get(self.path, 0) + 1
        location = b"https://" + HOST.encode() + b"/ok"
        responses = {
            "/ok": PREFIX + LENGTH + DATA,
            "/chunk": PREFIX + b"Transfer-Encoding: chunked\r\n\r\n"
                + b"2000;test=1\r\n" + DATA[:8192] + b"\r\n"
                + b"700\r\n" + DATA[8192:] + b"\r\n0\r\nEtag: fixture\r\n\r\n",
            "/truncated": PREFIX + LENGTH + DATA[:512],
            "/close": PREFIX + b"\r\n" + DATA,
            "/unclean": PREFIX + b"\r\n" + DATA,
            "/ambiguous": PREFIX + b"Transfer-Encoding: chunked\r\n" + LENGTH + DATA,
            "/dup": PREFIX + b"Content-Length: 1\r\n" + LENGTH + DATA,
            "/encoding": PREFIX + b"Content-Encoding: gzip\r\n" + LENGTH + DATA,
            "/chunk-end": PREFIX + b"Transfer-Encoding: chunked\r\n\r\n1\r\na\r\n2\r\nb",
            "/overline": PREFIX + b"X: " + (b"a" * 5000) + b"\r\n" + LENGTH + DATA,
            "/go": b"HTTP/1.1 301 Moved\r\nLocation: " + location + b"\r\n\r\nignored",
            "/relative": b"HTTP/1.1 308 Moved\r\nLocation: ok\r\n\r\nignored",
            "/loop": b"HTTP/1.1 302 Found\r\nLocation: /loop\r\n\r\nignored",
            "/downgrade": b"HTTP/1.1 307 Moved\r\nLocation: http://bad.invalid/x\r\n\r\nignored",
            "/missing": b"HTTP/1.1 404 Missing\r\n\r\nlogin page",
            "/limit": b"HTTP/1.1 429 Too Many Requests\r\n\r\nretry after",
        }
        if self.path.startswith("/temporary-"):
            code = self.path.split("-")[1]
            retry = b"Retry-After: 0" if self.path.endswith("-once") else b"Retry-After: 1"
            if self.path.endswith("-later"):
                retry = b"Retry-After: 60"
            if self.path.endswith("-date"):
                retry = b"Retry-After: Tue, 24 Nov 2026 00:00:00 GMT"
            body = b"HTTP/1.1 " + code.encode() + b" Temporary\r\n" + retry + b"\r\n\r\nerror page"
            if self.path.endswith("-once") and self.counts[self.path] == 2:
                body = responses["/ok"]
        else:
            body = responses[self.path]
        for at in range(0, len(body), 257):
            self.wfile.write(body[at:at+257])
        if self.path == "/close":
            try:
                self.connection.unwrap()
            except (ssl.SSLError, ConnectionError):
                pass


class QuietServer(ThreadingHTTPServer):
    def handle_error(self, *args):
        # Clients deliberately abandon rejected and oversized responses.
        pass


def test():
    Origin.counts.clear()
    subprocess.run(["make", "-s", "-j4", "-f", "config/media-tls.mk", "media-tls-host"],
                   cwd=ROOT, check=True)
    sha = json.loads((ROOT / "dependencies.json").read_text())["bearssl"]["commit"]
    host = ROOT / "build/bearssl" / ("host-" + sha)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        create_test_ca(work, host)
        subprocess.run([os.getenv("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-Iinclude", "-I.deps/fatfs/source", "-I.deps/bearssl/inc", "-I" + str(work),
                        "src/core/internet_http.c", "src/core/media_tls.c", "src/core/bba_sockets.c",
                        "tests/internet_fetch_host.c", str(host / "libbearssl.a"), "-o", work / "client"],
                       cwd=ROOT, check=True)
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(work / "server.pem", work / "server.key")
        with QuietServer(("127.0.0.1", 0), Origin) as server:
            server.socket = ctx.wrap_socket(server.socket, server_side=True)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            cases = [
                ("ok", OK, len(DATA), 0), ("chunk", OK, len(DATA), 0), ("close", OK, len(DATA), 0),
                ("go", OK, len(DATA), 0), ("relative", OK, len(DATA), 0),
                ("ok", LARGE, 200, 0), ("chunk", LARGE, 2048, 0), ("close", LARGE, 2048, 0),
                ("missing", MISSING, 300, 0), ("limit", LIMITED, 100, 0),
                ("ok", STOPPED, len(DATA), 1),
                *[(f"temporary-{code}-once", OK, len(DATA), 0) for code in (408, 502, 503, 504)],
                ("temporary-503-persistent", ERROR, len(DATA), 0),
                ("temporary-503-later", ERROR, len(DATA), 0),
                ("temporary-503-date", ERROR, len(DATA), 0),
                ("temporary-503-cancel", STOPPED, len(DATA), 0),
            ] + [(path, ERROR, len(DATA), 0) for path in
                 ("truncated", "unclean", "ambiguous", "dup", "encoding", "chunk-end", "overline", "loop", "downgrade")]
            cases = [(*case, 0) for case in cases] + [
                ("prefix-length", OK, 1024, 0, 1024), ("prefix-chunk", OK, 700, 0, 700),
                ("prefix-length", LARGE, 512, 0, 1024), ("prefix-chunk", LARGE, 512, 0, 1024),
                ("prefix-error", ERROR, 8192, 0, 4096), ("missing", MISSING, 1024, 0, 1),
            ]
            try:
                for path, expected, cap, cancel, prefix in cases:
                    output = work / "body"
                    env = os.environ.copy()
                    if path.endswith("-cancel"):
                        env["KUI_TEST_CANCEL_BACKOFF"] = "1"
                    reply = subprocess.run([work / "client", f"https://{HOST}/{path}", output, str(cap),
                                            str(server.server_port), str(cancel), str(prefix)], capture_output=True, timeout=10, env=env)
                    actual = tuple(reply.stdout.decode().strip().split(" ", 2))
                    assert reply.returncode == (0 if expected == OK else expected + 10), (path, actual, reply.stderr)
                    assert int(actual[0]) == expected
                    data = output.read_bytes()
                    assert len(data) <= cap, (path, len(data))
                    assert (b"ab" if path == "chunk-end" else DATA).startswith(data), path
                    if expected == OK:
                        assert data == (DATA[:cap] if prefix else DATA), path
                    if path.startswith("temporary-"):
                        attempts = 2 if path.endswith(("-once", "-persistent")) else 1
                        assert Origin.counts["/" + path] == attempts, path
                        if expected != OK:
                            assert not data, path
                    if path in ("missing", "limit", "ambiguous", "dup", "encoding", "overline", "loop", "downgrade"):
                        assert not data, path
            finally:
                server.shutdown()
                thread.join()
        print(f"{len(cases)} HTTPS transfer cases passed using production TLS and BBA sockets")


if __name__ == "__main__":
    test()
