#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare all three streamed thumbnails with the existing area/alpha filter."""
import binascii
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
MAGIC = b"\x89PNG\r\n\x1a\n"

def chunk(kind, data):
    return struct.pack(">I", len(data))+kind+data+struct.pack(">I", binascii.crc32(kind+data))

def paeth(a,b,c):
    preds = [a,b,c]
    return min(preds, key=lambda value: abs(a+b-c-value))

def encode(w,h,ch, *, change=lambda z:z, interlace=0):
    raw = bytearray()
    prev = bytes(w*ch)
    for y in range(h):
        row = bytes((x*7 + y*9 + (x>>2)*y)%256 for x in range(w*ch))
        raw.append(y%5)
        for x,value in enumerate(row):
            a = row[x-ch] if x>=ch else 0
            b = prev[x]
            c = prev[x-ch] if x>=ch else 0
            predictor = (0,a,b,(a+b)//2,paeth(a,b,c))[y%5]
            raw.append((value-predictor)%256)
        prev = row
    body=change(zlib.compress(raw))
    header = struct.pack(">IIBBBBB",w,h,8,2 if ch==3 else 6,0,0,interlace)
    # Split the Deflate stream independently of its blocks, rows and output window.
    return (MAGIC+chunk(b"IHDR",header)+b"".join(chunk(b"IDAT",body[i:i+937])
            for i in range(0,len(body),937))+chunk(b"IEND",b""))

with tempfile.TemporaryDirectory() as temp:
    png=Path(temp,"fixture.png")
    def check(data, expect):
        png.write_bytes(data)
        subprocess.run([ROOT/"build/cover-rows-host",png,expect],check=True)
    for width,height,channels in [(161,179,4),(512,170,3),(173,503,3),(639,1,4),(1,639,3)]:
        check(encode(width,height,channels),"match")
    good=encode(215,86,4)
    check(good,"cancel")
    check(good[:-1],"invalid")
    check(good[:-1]+bytes([good[-1]^255]),"invalid")
    check(good+b"x","invalid")
    check(encode(215,86,4,change=lambda z:z[:-1]),"invalid")
    check(encode(215,86,4,change=lambda z:z+b"x"),"invalid")
    check(encode(159,120,4),"fallback")
    check(encode(640,19,4),"match")
    check(encode(170,90,3,interlace=1),"fallback")
    check(good[:33]+chunk(b"tRNS",b"\0"*6)+good[33:],"invalid")
    # Paletted 4-bit and grayscale 16-bit sources use a separate small
    # expanded scanline. Verify both against the original decoder.
    w,h=173,222
    head=lambda bits,kind: MAGIC+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,bits,kind,0,0,0))
    def plain_rows(rows):
        return chunk(b"IDAT",zlib.compress(b"".join(b"\0"+row for row in rows)))+chunk(b"IEND",b"")
    colors=bytes((v*i)%256 for v in range(16) for i in (17,23,35))
    palette=(head(4,3)+chunk(b"PLTE",colors)+chunk(b"tRNS",bytes(range(0,256,16))))
    packed=plain_rows(bytes((x+y)%256 for x in range((w+1)//2)) for y in range(h))
    check(palette+packed,"match")
    check(head(4,0)+packed,"match")
    gray16=plain_rows(b"".join(struct.pack(">H",(x*913+y*579)%65536) for x in range(w)) for y in range(h))
    check(head(16,0)+gray16,"match")
    check(head(16,0)+chunk(b"tRNS",b"\0\0")+gray16,"match")
    rgb=encode(192,174,3)
    check(rgb[:33]+chunk(b"tRNS",b"\0"*6)+rgb[33:],"match")

# During unittest discovery argv belongs to the parent test runner.
if __name__ == "__main__":
    for path in sys.argv[1:]:
        subprocess.run([ROOT/"build/cover-rows-host",path,"match"],check=True)
