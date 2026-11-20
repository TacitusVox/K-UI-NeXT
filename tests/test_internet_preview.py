#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the storage converter on generated, redistributable motion frames."""
from array import array
from math import sqrt
from pathlib import Path
import struct
import subprocess
import tempfile
import sys
import unittest

BIN = Path("build/test-internet-preview").resolve()
FRAME = 160*120*2


class Preview(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.src = Path(self.temp.name) / "src.ogv"
        self.dst = Path(self.temp.name) / "dest.kpv"

    def encode(self, size="96x64", rate="30000/1001", duration="12", audio=False, audio_length=None):
        inputs = ["-f", "lavfi", "-i", f"testsrc2=s={size}:r={rate}"]
        if audio:
            signal = "sine=frequency=300:sample_rate=48000"
            if audio_length:
                signal += f":duration={audio_length}"
            inputs += ["-f", "lavfi", "-i", signal, "-ac", "2", "-c:a", "libvorbis"]
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", *inputs, "-t", duration,
                        "-c:v", "libtheora", "-threads", "1", str(self.src)], check=True)

    def run_converter(self, expected, *args):
        result = subprocess.run([str(BIN), str(self.src), str(self.dst), *args], capture_output=True)
        self.assertEqual(result.returncode == 0, expected, result.stderr)
        return result

    def read_frames(self, count, audio=False):
        data = self.dst.read_bytes()
        self.assertEqual(struct.unpack_from("<4sHHHHI", data), (b"KPV2",160,120,10,16000 if audio else 0,count))
        self.assertEqual(len(data), 16 + count*(FRAME + (3200 if audio else 0)))
        return [data[16+i*FRAME:16+(i+1)*FRAME] for i in range(count)]

    def test_rate_limit_mixed_audio(self):
        self.encode(audio=True)
        prefix = self.src.with_name("prefix.ogv")
        subprocess.run([str(BIN.with_name("test-preview-prefix")), self.src, prefix], check=True)
        self.assertLess(prefix.stat().st_size, self.src.stat().st_size)
        self.src = prefix
        self.run_converter(True)
        frames = self.read_frames(100, audio=True)
        self.assertGreater(len(set(frames)), 60)
        sound = array("h", self.dst.read_bytes()[16+100*FRAME:])
        reference = subprocess.check_output(["ffmpeg", "-loglevel", "error", "-i", self.src,
                    "-map", "0:a:0", "-ac", "1", "-ar", "16000", "-t", "10", "-f", "s16le", "-"])
        ref = array("h",reference)
        if sys.byteorder != "little":
            sound.byteswap()
            ref.byteswap()
        self.assertEqual(len(sound), len(ref))
        for start in (0, 75000, 153000):
            a, b = sound[start:start+6400], ref[start:start+6400]
            corr = sum(x*y for x,y in zip(a,b)) / sqrt(sum(x*x for x in a)*sum(y*y for y in b))
            self.assertGreater(corr, .98)

    def test_offset_keeps_video_and_sound_in_sync(self):
        # A unique brightness per source frame and a sweeping tone distinguish
        # frame or sample misalignment from a correctly shortened file.
        start = 18
        video = "nullsrc=s=160x120:r=30000/1001,geq=lum='24+mod(N*37,195)':cb=128:cr=128"
        signal = "aevalsrc=0.25*sin(2*PI*(75*t+11.3*t*t)):s=48000"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", video,
                        "-f", "lavfi", "-i", signal, "-t", "32", "-c:v", "libtheora",
                        "-c:a", "libvorbis", "-threads", "1", str(self.src)], check=True)
        original = self.src
        self.run_converter(True, "0", str(start))
        expected = self.dst.read_bytes()
        self.src = self.src.with_name("offset-prefix.ogv")
        subprocess.run([str(BIN.with_name("test-preview-prefix")), original, self.src,
                        str(start)], check=True)
        self.assertLess(self.src.stat().st_size, original.stat().st_size)
        self.run_converter(True, "0", str(start))
        self.assertEqual(self.dst.read_bytes(), expected)

        frames = self.read_frames(100, audio=True)
        # The presentation clock samples the frame containing each requested
        # instant. Use FFmpeg's independently decoded luminance for all frames.
        reference = subprocess.check_output(["ffmpeg", "-v", "error", "-i", original,
                    "-map", "0:v:0", "-vf", "scale=1:1", "-pix_fmt", "gray",
                    "-f", "rawvideo", "-threads", "1", "-"])
        for i, frame in enumerate(frames):
            sample = int.from_bytes(frame[:2], "little")
            green = ((sample >> 5) & 63) * 255 / 63
            self.assertAlmostEqual(green, reference[(start * 10 + i) * 30000 // 10010], delta=5)

        sound = array("h", expected[16 + 100*FRAME:])
        pcm = subprocess.check_output(["ffmpeg", "-v", "error", "-i", original,
                    "-map", "0:a:0", "-ac", "1", "-ar", "16000", "-f", "s16le", "-"])
        ref = array("h", pcm)[start * 16000:(start + 10) * 16000]
        if sys.byteorder != "little":
            sound.byteswap()
            ref.byteswap()
        self.assertEqual(len(ref), len(sound))
        for i in (0, 73000, 153000):
            a, b = sound[i:i+6400], ref[i:i+6400]
            corr = sum(x*y for x,y in zip(a,b)) / sqrt(sum(x*x for x in a)*sum(y*y for y in b))
            self.assertGreater(corr, .98)
        self.run_converter(False, "8", str(start))
        self.encode(rate="15", duration="2", audio=True)
        self.run_converter(False, "0", str(start))

    def test_short_audio_tail_is_silence(self):
        self.encode(rate="10", duration="2", audio=True, audio_length="0.6")
        self.run_converter(True)
        self.read_frames(20, audio=True)
        data = self.dst.read_bytes()[16+20*FRAME:]
        self.assertNotEqual(set(data[:12000]), {0})
        self.assertEqual(set(data[24000:]), {0})

    def test_short_and_duplicate(self):
        self.encode(rate="5", duration="1")
        self.run_converter(True)
        frames = self.read_frames(10)
        self.assertEqual(frames[0], frames[1])
        self.assertNotEqual(frames[0], frames[2])

    def test_stop_truncated_oversize(self):
        self.encode(duration="2")
        self.assertIn(b"Cancelled", self.run_converter(False, "3").stderr)
        for fault in ("short-read", "short-write"):
            self.assertIn(b"Card I/O failed", self.run_converter(False, "0", "0", fault).stderr)
        self.src.write_bytes(self.src.read_bytes()[:-73])
        self.assertIn(b"Unplayable source", self.run_converter(False).stderr)
        self.encode(size="704x480",duration="0.2")
        self.assertIn(b"Unplayable source", self.run_converter(False).stderr)


if __name__ == "__main__":
    unittest.main()
