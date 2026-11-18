# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the actual media conversion binaries on generated video/PDF fixtures."""
import io
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from media_formats import convert_manual, convert_preview


class MediaFormats(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.base = Path(temp.name)
        self.output = self.base / "media.bin"

    @unittest.skipUnless(shutil.which("ffmpeg"), "ffmpeg required")
    def test_preview(self):
        ffmpeg = shutil.which("ffmpeg")
        src = self.base / "source.mp4"
        subprocess.run([ffmpeg, "-nostdin", "-loglevel", "error", "-f", "lavfi",
                        "-i", "color=c=red:size=192x96:rate=10", "-t", "11",
                        "-threads", "1", "-c:v", "mpeg4", str(src)], check=True)
        convert_preview(src, self.output)
        result = self.output.read_bytes()
        self.assertEqual(struct.unpack_from("<4sHHHHI", result), (b"KPV1",160,120,10,0,100))
        self.assertEqual(len(result), 16+160*120*2*100)
        for index in (0, 99):
            frame = 16+index*160*120*2
            top = struct.unpack_from("<H", result, frame+80*2)[0]
            center = struct.unpack_from("<H", result, frame+(60*160+80)*2)[0]
            self.assertLess(top, 0x21)  # Dithered near-black letterbox.
            self.assertGreater(center >> 11, 28)
            self.assertLess(center & 0x07ff, 0x42)

    @unittest.skipUnless(shutil.which("pdftoppm") and shutil.which("pdfinfo"), "Poppler required")
    def test_manual(self):
        from PIL import Image
        src = self.base / "book.pdf"
        images = [Image.new("RGB", size, color) for size, color in
                  [((800, 1200), "red"), ((1000, 500), "blue")]]
        images[0].save(src, save_all=True, append_images=images[1:])
        convert_manual(src, self.output)
        result = self.output.read_bytes()
        self.assertEqual(struct.unpack_from("<4sHH",result), (b"KDM1",2,0))
        start, second, end = struct.unpack_from("<3I",result,8)
        self.assertEqual(start, 20)
        self.assertEqual(end, len(result))
        for n, (left, right) in enumerate(((start,second), (second,end))):
            with Image.open(io.BytesIO(result[left:right])) as image:
                self.assertEqual(image.format, "JPEG")
                self.assertEqual(max(image.size), 1024)
                pixel = image.convert("RGB").getpixel((100,100))
                self.assertGreater(pixel[2 if n else 0], 200)
                self.assertLess(pixel[0 if n else 2], 50)

    def test_failure_preserves_destination(self):
        invalid = self.base / "broken"
        invalid.write_bytes(b"truncated source")
        self.output.write_bytes(b"prior")
        for convert in (convert_manual, convert_preview):
            with self.assertRaises(ValueError):
                convert(invalid, self.output)
            self.assertEqual(self.output.read_bytes(), b"prior")


if __name__ == "__main__":
    unittest.main()
