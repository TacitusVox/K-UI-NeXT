# SPDX-License-Identifier: GPL-3.0-only
"""From a synthetic Dreamcast disc through the real LAN companion back to a card."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from PIL import Image
from games_fixture import make_fixture
from test_games_covers_images import write_manifest
from test_games_library_images import private_formatter

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(args, check=True, cwd=ROOT, timeout=100, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


def main():
    with tempfile.TemporaryDirectory(prefix="kui-bba-media-") as tmp:
        folder = Path(tmp)
        make_fixture(folder / "Games" / "Local Game", b"SYNTHETIC RACING GAME")
        (folder / "KUI" / "covers").mkdir(parents=True)
        write_manifest(folder)
        # Real media files made for this test, looked up using the disc IP data.
        Image.new("RGB", (64, 64), "#ff0000").save(folder / "box.png")
        pages = [Image.new("RGB", (80, 120), color) for color in ("blue", "yellow")]
        pages[0].save(folder / "manual.pdf", save_all=True, append_images=pages[1:])
        run("ffmpeg", "-v", "error", "-f", "lavfi", "-i", "color=c=red:s=64x48:r=10",
            "-t", "1", "-threads", "1", "-c:v", "mpeg4", str(folder / "clip.mp4"))
        gamelist = folder / "gamelist.xml"
        gamelist.write_text("<gameList><game><name>Synthetic Racing Game</name>"
                            "<serial>KUI-TEST</serial><image>box.png</image>"
                            "<manual>manual.pdf</manual><video>clip.mp4</video></game></gameList>")
        formatter = private_formatter(folder)
        proc = subprocess.Popen([sys.executable, str(ROOT / "tools/media_server.py"), "--offline",
                                 "--gamelist", str(gamelist), "--port", "0"], stdout=subprocess.PIPE,
                                text=True)
        try:
            address = proc.stdout.readline()
            assert address.startswith("K-UI media companion at 127.0.0.1:"), address
            port = str(int(address.split(":")[-1]))
            for kind in ("fat32", "exfat"):
                if kind == "exfat":
                    # IP.BIN uses the first peripheral bit to identify Windows CE.
                    track = folder / "Games/Local Game/track03.bin"
                    with track.open("r+b") as data:
                        data.seek(16 + 56)
                        data.write(b"0000001 ")
                card = folder / (kind + ".img")
                run(formatter, kind, card)
                run(ROOT / "build/games-covers-image", card, folder, "seed", "network-media")
                run(ROOT / "build/bba-media-integration", card, port)
        finally:
            proc.terminate()
            proc.wait(timeout=10)
            proc.stdout.close()


if __name__ == "__main__":
    main()
