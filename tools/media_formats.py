# SPDX-License-Identifier: GPL-3.0-only
"""Turn a local video or PDF into a file the K-UI Dreamcast viewer can stream."""

import contextlib
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import time


@contextlib.contextmanager
def _output(destination):
    destination = Path(destination)
    with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as temp:
        name = temp.name
    try:
        with open(name, "w+b") as out:
            yield out
        os.replace(name, destination)
    finally:
        Path(name).unlink(missing_ok=True)


def _convert(args, timeout):
    try:
        # No subprocess can consume the companion's terminal input.
        proc = subprocess.run(args, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, timeout=timeout, check=False,
                              env={**os.environ, "LC_ALL": "C"})
    except FileNotFoundError as error:
        raise ValueError(f"Install {args[0]} on the media companion") from error
    except subprocess.TimeoutExpired as error:
        raise ValueError(f"{args[0]} exceeded its conversion deadline") from error
    if proc.returncode:
        raise ValueError(f"{args[0]} could not decode this file")
    return proc.stdout


def convert_preview(source_path, dest_path, *, legacy=False):
    """Convert up to ten seconds of video with an optional 16 kHz mono soundtrack."""
    width, height, fps, max_frames = 160, 120, 10, 100
    frame_bytes = width * height * 2
    # Accept common self-contained movies. Disable playlists and network protocols
    # so embedded references cannot make the converter fetch additional files.
    # The input path is absolute, including when a user puts '-' in a filename.
    filters = ("fps=10,scale=w='if(gt(dar,4/3),160,max(1,trunc(120*dar)))':"
               "h='if(gt(dar,4/3),max(1,trunc(160/dar)),120)',setsar=1,"
               "pad=160:120:(ow-iw)/2:(oh-ih)/2:black")
    source = ["-protocol_whitelist", "file", "-format_whitelist",
              "mov,avi,matroska,webm,mpeg,mpegts,ogg,flv", "-i", str(Path(source_path).resolve())]
    ffmpeg = ["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-xerror",
              "-threads", "1", "-filter_threads", "1", "-max_pixels", "16777216"]
    raw = _convert([
        *ffmpeg, *source, "-map", "0:v:0", "-an", "-sn", "-dn",
        "-vf", filters, "-frames:v", str(max_frames), "-threads", "1",
        "-pix_fmt", "rgb565le", "-f", "rawvideo", "pipe:1"
    ], 90)
    count, remainder = divmod(len(raw), frame_bytes)
    if not 1 <= count <= max_frames or remainder:
        raise ValueError("The video did not produce complete preview frames")
    audio = b""
    sample_rate = 16000
    if not legacy:
        audio_stream = _convert(["ffprobe", "-v", "error", "-select_streams", "a:0",
                                 "-show_entries", "stream=index", "-of", "csv=p=0", *source], 15)
        if audio_stream.strip():
            # Preserve the timing of tracks that begin after the first video frame.
            pcm = _convert([*ffmpeg, *source, "-map", "0:a:0", "-vn", "-sn", "-dn",
                            "-t", str(count / fps), "-af", "aresample=async=1:first_pts=0",
                            "-ac", "1", "-ar", str(sample_rate), "-f", "s16le", "pipe:1"], 30)
            if pcm:
                total = count * (sample_rate // fps) * 2
                audio = pcm[:total].ljust(total, b"\0")
    with _output(dest_path) as out:
        out.write(struct.pack("<4sHHHHI", b"KPV1" if legacy else b"KPV2", width,
                              height, fps, sample_rate if audio else 0, count))
        out.write(raw)
        out.write(audio)


def convert_manual(source_path, dest_path):
    """Rasterize every PDF page. The Dreamcast reads and decodes one JPEG at a time."""
    source = str(Path(source_path).resolve())
    info = _convert(["pdfinfo", source], 15)
    match = re.search(rb"^Pages:\s+(\d+)\s*$", info, re.MULTILINE)
    if not match or not 1 <= (pages := int(match.group(1))) <= 256:
        raise ValueError("Manual must contain 1 to 256 pages")
    deadline = time.monotonic() + 180
    with _output(dest_path) as out:
        out.write(struct.pack("<4sHH", b"KDM1", pages, 0))
        out.write(bytes(4 * (pages+1)))
        offsets = [out.tell()]
        for page in range(1, pages + 1):
            seconds = deadline - time.monotonic()
            if seconds <= 0:
                raise ValueError("Manual exceeded its conversion deadline")
            jpg = _convert(["pdftoppm", "-f", str(page), "-l", str(page), "-singlefile",
                            "-scale-to", "1024", "-jpeg", "-jpegopt", "quality=82",
                            source], min(seconds, 30))
            if not (jpg.startswith(b"\xff\xd8") and jpg.endswith(b"\xff\xd9")):
                raise ValueError(f"Cannot render manual page {page}")
            if len(jpg) > 512 * 1024 or out.tell() + len(jpg) > 32 * 1024 * 1024:
                raise ValueError("Manual exceeds the Dreamcast page or file size limit")
            out.write(jpg)
            offsets.append(out.tell())
        out.seek(8)
        out.write(struct.pack(f"<{len(offsets)}I", *offsets))
