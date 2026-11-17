# SPDX-License-Identifier: GPL-3.0-only
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("build_splash",ROOT/"tools/build_splash.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class StartupAssets(unittest.TestCase):
    def blob(self,data):
        return hashlib.sha1(b"blob "+str(len(data)).encode()+b"\0"+data).hexdigest()
    def exact_rgb565(self,data):
        """Every pixel is already an RGB565 value, so encoding loses nothing."""
        from PIL import Image
        import io
        image=Image.open(io.BytesIO(data))
        self.assertEqual((image.size,image.mode),((640,480),"RGB"))
        pixels=module.decode_png(data)
        expanded=[(((v>>11)<<3)|((v>>11)>>2),(((v>>5)&63)<<2)|(((v>>5)&63)>>4),((v&31)<<3)|((v&31)>>2))
            for v in pixels]
        self.assertEqual(expanded,list(image.getdata()))
        return pixels
    def test_separate_boot_and_runtime_art(self):
        """The runtime shows the Muramasa splash; the boot disc keeps its own,
        version-agnostic Bootloader v3 art under the historical boot-red name."""
        startup=(ROOT/"resources/branding/startup.png").read_bytes()
        boot=(ROOT/"resources/branding/boot-red.png").read_bytes()
        self.assertEqual(self.blob(startup),module.PNG_BLOB)
        self.assertEqual(self.blob(boot),module.BOOT_PNG_BLOB)
        self.assertNotEqual(startup,boot)
        for data in (startup,boot):
            pixels=self.exact_rgb565(data)
            self.assertEqual(len(pixels),640*480)
            self.assertGreater(len(set(pixels)),256)
    def test_decoder_rejects_damage(self):
        data=(ROOT/"resources/branding/startup.png").read_bytes()
        bad=bytearray(data);bad[len(bad)//2]^=1
        with self.assertRaises(ValueError):module.decode_png(bad)
        with self.assertRaises(ValueError):module.decode_png(data[:32])
    def test_shortened_original_chime(self):
        samples=list(module.startup_samples())
        self.assertEqual(len(samples),116865)
        self.assertLessEqual(len(samples)/44100,2.65)
        self.assertEqual(samples[0],0)
        self.assertEqual(samples[-1],0)
        self.assertLessEqual(max(map(abs,samples)),26000)
        self.assertTrue(any(samples[10000:20000]))
        # Fade reaches silence before the deadline, without the old long tail.
        self.assertTrue(any(samples[2*44100:]))
        self.assertFalse(any(samples[round(2.62*44100):]))
    def test_embedded_chime_encoding(self):
        """The runtime embeds this Ogg; it decodes to exactly these notes."""
        data=module.CHIME.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(),module.CHIME_SHA256)
        self.assertLess(len(data),32*1024)
        checker=ROOT/"build/music-asset-check"
        self.assertTrue(checker.exists(),"build/music-asset-check is missing; run make test")
        with tempfile.TemporaryDirectory(prefix="kui-chime-") as temp:
            wav=Path(temp)/"startup-chime.wav"
            module.write_chime_wav(wav)
            result=subprocess.run([str(checker),str(module.CHIME),str(wav)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn(f"{module.STARTUP_FRAMES} frames at {module.STARTUP_RATE} Hz",result.stdout)
if __name__=="__main__":
    unittest.main()
