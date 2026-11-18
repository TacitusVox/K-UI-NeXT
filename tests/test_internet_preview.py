#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the storage converter on generated, redistributable motion frames."""
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

BIN = Path("build/test-internet-preview").resolve()
FRAME = 160*120*2


class Preview(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.src = Path(self.temp.name) / "src.ogv"
        self.dst = Path(self.temp.name) / "dest.kpv"

    def encode(self, size="96x64", rate="30000/1001", duration="12", audio=False):
        inputs = ["-f", "lavfi", "-i", f"testsrc2=s={size}:r={rate}"]
        if audio:
            inputs += ["-f", "lavfi", "-i", "sine=frequency=300", "-c:a", "libvorbis"]
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", *inputs, "-t", duration,
                        "-c:v", "libtheora", "-threads", "1", str(self.src)], check=True)

    def run_converter(self, expected, *args):
        result = subprocess.run([str(BIN), str(self.src), str(self.dst), *args], capture_output=True)
        self.assertEqual(result.returncode == 0, expected, result.stderr)
        return result

    def read_frames(self, count):
        data = self.dst.read_bytes()
        self.assertEqual(struct.unpack_from("<4sHHHHI", data), (b"KPV1",160,120,10,0,count))
        self.assertEqual(len(data), 16 + count*FRAME)
        return [data[16+i*FRAME:16+(i+1)*FRAME] for i in range(count)]

    def test_rate_limit_mixed_audio(self):
        self.encode(audio=True)
        self.run_converter(True)
        frames = self.read_frames(100)
        self.assertGreater(len(set(frames)), 60)

    def test_short_and_duplicate(self):
        self.encode(rate="5", duration="1")
        self.run_converter(True)
        frames = self.read_frames(10)
        self.assertEqual(frames[0], frames[1])
        self.assertNotEqual(frames[0], frames[2])

    def test_stop_truncated_oversize(self):
        self.encode(duration="2")
        self.run_converter(False, "3")
        self.src.write_bytes(self.src.read_bytes()[:-73])
        self.run_converter(False)
        self.encode(size="704x480",duration="0.2")
        self.run_converter(False)


if __name__ == "__main__":
    unittest.main()
