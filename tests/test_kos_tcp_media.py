#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run the exact patched KOS TCP close/ACK/send/poll/timer routines on host."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
KOS = Path(os.environ.get("KOS_BASE", ROOT / ".deps/kos"))
FUNCTIONS = (
    "tcp_send_fin_ack", "tcp_send_ack", "tcp_send_data", "tcp_begin_close",
    "tcp_enter_time_wait", "net_tcp_close", "net_tcp_sendto", "net_tcp_poll",
    "tcp_ooo_add", "tcp_ooo_consume", "process_pkt", "net_tcp_job", "tcp_header_valid",
)


def function(src, name):
    pattern = r"^static [\w *]+\b" + name + r"\([^;{}]+\)\s*\{.*?^\}"
    found = re.search(pattern, src, flags=re.M | re.S)
    if not found:
        raise RuntimeError("Missing pinned KOS function " + name)
    return found.group()


def main():
    with tempfile.TemporaryDirectory() as temp:
        dest = Path(temp)
        patched = dest / "tcp.c"
        subprocess.run(["patch", "--batch", "--fuzz=0", "-o", str(patched),
                        str(KOS / "kernel/net/net_tcp.c"),
                        str(ROOT / "tools/patches/kos-tcp-media.patch")], check=True)
        src = patched.read_text()
        types = src[src.index("typedef struct tcp_hdr {"):src.index("/* Forward declarations */")]
        (dest / "kos_tcp_types.h").write_text(types)
        (dest / "kos_tcp_routines.h").write_text("\n".join(function(src,n) for n in FUNCTIONS))
        binary = dest / "tcp-test"
        subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-g", "-O1",
                        "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-I"+temp, str(ROOT/"tests/kos_tcp_media_host.c"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


if __name__ == "__main__":
    main()
