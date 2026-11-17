#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the production TLS engine with short nonblocking reads and writes.

Generate an ephemeral test CA in a temporary directory. The production anchor
set is linked into a separate client to check that it rejects this certificate.
"""
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]
HOST = "media.example.invalid"
PAYLOAD = bytes(range(256)) * 85
RESPONSE = f"HTTP/1.1 200 OK\r\nContent-Length: {len(PAYLOAD)}\r\n\r\n".encode() + PAYLOAD


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def create_test_ca(work, host_build, hostname=HOST):
    def openssl(*args):
        run("openssl", *args, cwd=work, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)

    openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
            "-subj", "/CN=K-UI test CA", "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign", "-keyout", "ca.key", "-out", "ca.pem")
    openssl("req", "-new", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=" + hostname,
            "-keyout", "server.key", "-out", "server.csr")
    (work / "server.ext").write_text("subjectAltName=DNS:" + hostname + "\n"
                                     "basicConstraints=critical,CA:FALSE\n"
                                     "extendedKeyUsage=serverAuth\n"
                                     "keyUsage=digitalSignature\n")
    openssl("x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key",
            "-CAcreateserial", "-days", "1", "-sha256", "-extfile", "server.ext",
            "-out", "server.pem")
    with (work / "media_tls_roots.h").open("wb") as roots:
        run(host_build / "brssl", "ta", "-q", work / "ca.pem", stdout=roots)


def test():
    run("make", "-s", "-j4", "-f", "config/media-tls.mk", "media-tls-host", cwd=ROOT)
    pin = json.loads((ROOT / "dependencies.json").read_text())["bearssl"]["commit"]
    host_build = ROOT / "build/bearssl" / ("host-" + pin)
    with tempfile.TemporaryDirectory() as temp:
        work = Path(temp)
        create_test_ca(work, host_build)

        for name, roots in (("private-client", work), ("production-client", ROOT / "build/bearssl")):
            run(os.getenv("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-Iinclude", "-I.deps/bearssl/inc", "-I" + str(roots), "src/core/media_tls.c",
                "tests/test_media_tls.c", str(host_build / "libbearssl.a"),
                "-Wl,--wrap=time", "-o", work / name, cwd=ROOT)

        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(work / "server.pem", work / "server.key")
        cases = [
            ("valid", "private-client", HOST, None, None),
            ("hostname", "private-client", "another.example.invalid", None, b"hostname mismatch"),
            ("unknown CA", "production-client", HOST, None, b"authority untrusted"),
            ("clock", "private-client", HOST, "946684800", b"certificate date"),
            ("truncation", "private-client", HOST, None, b"ended early"),
        ]
        for name, client, hostname, timestamp, expected in cases:
            with socket.socket() as listener:
                listener.bind(("127.0.0.1", 0))
                listener.listen()
                listener.settimeout(4)
                observed_names = []
                errors = []
                context.set_servername_callback(lambda sock, sni, ctx: observed_names.append(sni))

                def serve():
                    try:
                        conn, _ = listener.accept()
                        conn.settimeout(6)
                        with conn, context.wrap_socket(conn, server_side=True) as stream:
                            request = b""
                            while not request.endswith(b"\r\n\r\n"):
                                chunk = stream.recv(512)
                                if not chunk:
                                    raise ValueError("client closed during HTTP request")
                                request += chunk
                            assert request.startswith(b"GET /cover HTTP/1.1\r\nHost: " + HOST.encode())
                            stream.sendall(RESPONSE)
                            if name == "valid":
                                stream.unwrap().close()  # authenticated close_notify
                    except (ssl.SSLError, ConnectionError) as exc:
                        if expected is None:
                            errors.append(exc)
                    except Exception as exc:
                        errors.append(exc)

                server = threading.Thread(target=serve)
                server.start()
                env = os.environ.copy()
                env.pop("KUI_TEST_TLS_TIME", None)
                if timestamp:
                    env["KUI_TEST_TLS_TIME"] = timestamp
                result = subprocess.run([work / client, hostname, str(listener.getsockname()[1])],
                                        env=env, capture_output=True, timeout=10)
                server.join(8)
                assert not server.is_alive() and not errors, (name, errors)
                assert observed_names == [hostname], (name, observed_names)
                if expected:
                    assert result.returncode == 1 and expected in result.stderr, (name, result.stderr)
                    if name != "truncation":
                        assert not result.stdout
                else:
                    assert result.returncode == 0 and result.stdout == RESPONSE, (name, result.stderr)
            print("media TLS:", name, "passed")


if __name__ == "__main__":
    test()
