#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run production HTTP, TLS, and BBA socket code against loopback TLS origins."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import hashlib
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
OK, MISSING, LIMITED, STOPPED, ERROR, LARGE, RANGE_IGNORED, CONNECT_FAILED, NOT_MODIFIED = range(9)


ALT_HOST = "other.example.invalid"
DATE = "Sat, 14 Nov 2026 01:02:03 GMT"


class Origin(BaseHTTPRequestHandler):
    counts = {}
    validation_log = []
    responses = []

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path.startswith("/art-"):
            self.validation_log.append((self.headers["Host"], self.path, self.headers.get("If-None-Match"),
                                        self.headers.get("If-Modified-Since"), self.headers.get("X-Bad")))
            self.wfile.write(self.responses.pop(0))
            return
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
        if self.path.startswith("/range"):
            assert self.headers["Range"] == "bytes=1024-"
            tail = DATA[1024:]
            range_header = f"Content-Range: bytes 1024-{len(DATA)-1}/{len(DATA)}\r\n".encode()
            partial = b"HTTP/1.1 206 Partial Content\r\n"
            length = f"Content-Length: {len(tail)}\r\n\r\n".encode()
            body = {
                "/range": partial + range_header + length + tail,
                "/range-go": b"HTTP/1.1 302 Found\r\nLocation: /range\r\n\r\n",
                "/range-release": b"HTTP/1.1 302 Found\r\nLocation: /range?" + b"token=" + b"x"*2400 + b"\r\n\r\n",
                "/range-ignored": responses["/ok"],
                "/range-missing": partial + length + tail,
                "/range-bad": partial + range_header.replace(b"1024-", b"1025-") + length + tail,
                "/range-wrong-size": partial + range_header + LENGTH + tail,
                "/range-duplicate": partial + range_header*2 + length + tail,
                "/range-truncated": partial + range_header + length + tail[:100],
            }[self.path.split("?", 1)[0]]
        elif self.path.startswith("/temporary-"):
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


def validation_cases(work, server):
    validators = work / "validators"
    output = work / "art-body"
    base = f"https://{HOST}"
    alt = f"https://{ALT_HOST}"
    a, b = '"revision-A"', '"revision-B"'
    image = b"old image"
    changed = b"changed image"
    count = 0

    def headers(tag=None, date=None):
        return ((f"ETag: {tag}\r\n" if tag else "") +
                (f"Last-Modified: {date}\r\n" if date else "")).encode()

    def ok(data=image, tag=a, date=DATE):
        return PREFIX + headers(tag, date) + f"Content-Length: {len(data)}\r\n\r\n".encode() + data

    def same(extra=b""):
        return b"HTTP/1.1 304 Not Modified\r\n" + extra + b"Content-Length: 900000\r\n\r\n"

    def redirect(url):
        return f"HTTP/1.1 302 Found\r\nLocation: {url}\r\n".encode() + headers('"redirect"') + b"\r\n"

    def run(url, responses, expected_requests, *, result=OK, body=image, cap=1024, cancel=0):
        nonlocal count
        previous = validators.read_bytes() if validators.exists() else None
        Origin.responses[:] = responses
        Origin.validation_log.clear()
        r = subprocess.run([work / "client", url, output, str(cap), str(server.server_port), str(cancel)],
                           env=dict(os.environ, KUI_TEST_VALIDATORS=str(validators)), capture_output=True, timeout=10)
        assert r.returncode == (0 if result == OK else result + 10), (r.stdout, r.stderr)
        assert output.read_bytes() == body
        assert Origin.validation_log == [(*item, None) for item in expected_requests], Origin.validation_log
        assert not Origin.responses
        if result not in (OK, NOT_MODIFIED):
            assert validators.read_bytes() == previous
        count += 1

    resource = base + "/art-a"
    request = lambda tag=None, date=None: (HOST, "/art-a", tag, date)
    run(resource, [ok()], [request()])
    raw = validators.read_bytes()
    assert len(raw) == 224 and raw[:128].split(b"\0")[0] == a.encode()
    assert raw[128:192].split(b"\0")[0] == DATE.encode() and raw[-32:] == hashlib.sha256(resource.encode()).digest()
    run(resource, [same()], [request(a)], result=NOT_MODIFIED, body=b"", cap=1)
    assert validators.read_bytes() == raw
    # An ignored condition with an identical strong tag can stop after the
    # headers. The listener advertises 800 bytes and sends no body at all.
    run(resource, [PREFIX + headers(a) + b"Content-Length: 800\r\n\r\n"], [request(a)],result=NOT_MODIFIED,body=b"")
    run(resource, [same(headers("W/" + a))], [request(a)], result=NOT_MODIFIED, body=b"")
    run(resource, [ok(changed, "W/" + a)], [request("W/" + a)], body=changed)
    run(resource, [ok(changed, b)], [request("W/" + a)], body=changed)
    run(resource, [same(headers(a)), ok()], [request(b), request()])
    run(resource, [b"HTTP/1.1 412 Precondition Failed\r\n\r\n", ok(tag=None)], [request(a), request()])
    run(resource, [same()], [request(date=DATE)], result=NOT_MODIFIED, body=b"")
    run(resource, [same(headers(date="Sun, 15 Nov 2026 01:02:03 GMT")), ok(tag=None)],
        [request(date=DATE),request()])
    # A timestamp alone on a 200 cannot distinguish equal-length revisions.
    run(resource, [ok(b"new image",None,DATE)], [request(date=DATE)],body=b"new image")
    run(resource, [ok(tag="*", date="never")], [request(date=DATE)])
    assert validators.read_bytes()[:192] == bytes(192)
    run(resource, [same()], [request()], result=ERROR, body=b"")  # unsolicited 304
    run(resource, [ok(tag='"' + 'x'*128 + '"', date=None)], [request()])
    assert validators.read_bytes()[:192] == bytes(192)
    run(resource, [PREFIX + headers(a) + headers(b) + b"Content-Length: 1\r\n\r\nx"], [request()], body=b"x")
    assert validators.read_bytes()[:192] == bytes(192)
    # Reject persisted control bytes and missing string terminators before generating headers.
    validators.write_bytes(b'x'*128 + (b"bad\r\nX-Bad: yes").ljust(64,b"\0") + raw[-32:])
    run(resource, [ok()], [request()])
    # Never attach the first URL's validators to a different path or host.
    run(resource, [redirect("/art-new"), ok(changed,b)], [request(a), (HOST,"/art-new",None,None)], body=changed)
    run(resource, [redirect("/art-new"), same()], [request(),(HOST,"/art-new",b,None)], result=NOT_MODIFIED,body=b"")
    run(resource, [redirect(alt+"/art-new"),ok()], [request(),(ALT_HOST,"/art-new",None,None)])
    run(resource, [redirect(alt+"/art-new"),same()], [request(),(ALT_HOST,"/art-new",a,None)],result=NOT_MODIFIED,body=b"")
    run(resource, [ok()], [request()])
    run(resource, [ok(changed,b)], [request(a)], result=LARGE, body=b"", cap=1)
    run(resource, [PREFIX + headers(b) + b"Content-Length: 99\r\n\r\nx"], [request(a)], result=ERROR, body=b"x")
    run(resource, [ok(changed,b)], [request(a)],result=STOPPED,body=changed,cancel=1)
    return count


def test():
    Origin.counts.clear()
    subprocess.run(["make", "-s", "-j4", "-f", "config/media-tls.mk", "media-tls-host"],
                   cwd=ROOT, check=True)
    sha = json.loads((ROOT / "dependencies.json").read_text())["bearssl"]["commit"]
    host = ROOT / "build/bearssl" / ("host-" + sha)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        create_test_ca(work, host, alternate=ALT_HOST)
        subprocess.run([os.getenv("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-Iinclude", "-I.deps/fatfs/source", "-I.deps/bearssl/inc", "-I" + str(work),
                        "src/core/internet_http.c", "src/core/hash.c", "src/core/data.c", "src/core/media_tls.c", "src/core/bba_sockets.c",
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
                ("range", OK, len(DATA), 0), ("range-go", OK, len(DATA), 0),
                ("range-release", OK, len(DATA), 0),
                ("range-ignored", RANGE_IGNORED, len(DATA), 0),
                *[(name, ERROR, len(DATA), 0) for name in ("range-bad", "range-missing", "range-duplicate",
                                                          "range-wrong-size", "range-truncated")],
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
                    if path.startswith("range"):
                        env["KUI_TEST_RANGE"] = "1024"
                    if path.endswith("-cancel"):
                        env["KUI_TEST_CANCEL_BACKOFF"] = "1"
                    reply = subprocess.run([work / "client", f"https://{HOST}/{path}", output, str(cap),
                                            str(server.server_port), str(cancel), str(prefix)], capture_output=True, timeout=10, env=env)
                    actual = tuple(reply.stdout.decode().strip().split(" ", 2))
                    assert reply.returncode == (0 if expected == OK else expected + 10), (path, actual, reply.stderr)
                    assert int(actual[0]) == expected
                    data = output.read_bytes()
                    assert len(data) <= cap, (path, len(data))
                    reference = DATA[1024:] if path.startswith("range") else DATA
                    if path in ("range-ignored", "range-bad", "range-missing", "range-duplicate", "range-wrong-size"):
                        assert not data, path
                    assert (b"ab" if path == "chunk-end" else reference).startswith(data), path
                    if expected == OK:
                        assert data == (reference[:cap] if prefix else reference), path
                    if path.startswith("temporary-"):
                        attempts = 2 if path.endswith(("-once", "-persistent")) else 1
                        assert Origin.counts["/" + path] == attempts, path
                        if expected != OK:
                            assert not data, path
                    if path in ("missing", "limit", "ambiguous", "dup", "encoding", "overline", "loop", "downgrade"):
                        assert not data, path
                # Only 127.0.0.1 listens. The first A record reaches a real
                # refused TCP socket, then the fresh DNS lookup can recover.
                for mode, expected, attempts in (("transient", OK, 2), ("persistent", CONNECT_FAILED, 2),
                                                 ("cancel", STOPPED, 1), ("link-down", ERROR, 1)):
                    env = dict(os.environ, KUI_TEST_CONNECT=mode, KUI_TEST_ATTEMPTS=str(attempts))
                    if mode == "cancel":
                        env["KUI_TEST_CANCEL_BACKOFF"] = "1"
                    if mode == "link-down":
                        env["KUI_TEST_DROP_LINK"] = "1"
                    output = work / "connection-body"
                    reply = subprocess.run([work / "client", f"https://{HOST}/ok", output, str(len(DATA)),
                                            str(server.server_port)], env=env, capture_output=True, timeout=10)
                    assert reply.returncode == (0 if expected == OK else expected + 10), (mode, reply.stdout, reply.stderr)
                    assert output.read_bytes() == (DATA if expected == OK else b"")
                    if mode == "persistent":
                        assert f"{HOST}:443".encode() in reply.stdout
                validations = validation_cases(work, server)
            finally:
                server.shutdown()
                thread.join()
        print(f"{len(cases)+4+validations} HTTPS transfer cases passed using production TLS and BBA sockets")


if __name__ == "__main__":
    test()
