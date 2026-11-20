#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the early delayed Maple startup against an externally supplied owner image."""
import argparse
import ctypes as ct
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

from test_vmu_native_heap_execution import SH

ROOT = Path(__file__).resolve().parents[1]
LOAD = 0x8c010000
ENTRY = 0x8c0691c8
HEAP_CALL = 0x8c06921a
MAPLE_CALL = 0x8c069246
LOCK = 0x8c0a8144
SHA = 'ac5826c9a371de04207748e60e49e0a3c13a39468902f89d8c1c82e1f544bb48'


def host_library(folder):
    so = Path(folder) / 'startup.so'
    sources = ('vmu_native_discovery', 'vmu_native_layout', 'hash', 'data')
    subprocess.run(['cc', '-std=c11', '-O2', '-fPIC', '-shared', '-Wall', '-Wextra',
                    '-Werror', '-Wpedantic', '-I', str(ROOT/'include'),
                    *[str(ROOT/'src/core'/f'{name}.c') for name in sources], '-o', str(so)],
                   check=True)
    lib = ct.CDLL(str(so))
    pointer, length, word = ct.c_void_p, ct.c_size_t, ct.c_uint32
    lib.kui_vmu_native_discover.argtypes = (pointer, length, word, pointer)
    lib.kui_vmu_native_discover.restype = ct.c_int
    lib.kui_vmu_native_discover_layout.argtypes = (pointer, length, word, pointer, pointer)
    lib.kui_vmu_native_discover_layout.restype = ct.c_bool
    lib.kui_vmu_native_early_p2_calls_valid.argtypes = (pointer, length, pointer, word, word)
    lib.kui_vmu_native_early_p2_calls_valid.restype = ct.c_bool
    return lib


def original_instructions(source):
    for sr, args in ((0x60000000,(0,1,2)), (0x60000031,(4,5,6)), (0x400000a0,(11,12,13))):
        cpu = SH(source)
        cpu.r[15] = 0x8c00efc0
        cpu.sr = sr
        cpu.r[4:7] = args
        for pc in range(ENTRY, ENTRY+42, 2):
            cpu.plain(cpu.read(pc,2), pc)
        assert cpu.read(LOCK) == (LOCK+4)|0x20000000
        assert cpu.read(cpu.r[15]) == args[0]
        assert cpu.read(cpu.r[15]+8) == args[1]
        assert cpu.read(cpu.r[15]+4) == args[2]
        assert cpu.sr == (sr&~0xf0)|0xf0
        assert len([w for w in cpu.writes if w[1]==LOCK]) == 1
        assert cpu.block(LOAD,LOCK-LOAD) == source[:LOCK-LOAD]


def check(source,lib):
    driver, layout = ct.create_string_buffer(104), ct.create_string_buffer(440)
    assert lib.kui_vmu_native_discover_layout(source,len(source),LOAD,driver,layout)
    assert struct.unpack_from('<I',driver,8)[0] == 3
    assert struct.unpack_from('<I',layout,16)[0] == MAPLE_CALL

    def valid(blob,heap=HEAP_CALL,maple=MAPLE_CALL):
        return lib.kui_vmu_native_early_p2_calls_valid(blob,len(blob),driver,heap,maple)

    assert valid(source)
    assert not valid(source,heap=MAPLE_CALL-26)
    assert not valid(source,maple=HEAP_CALL+26)
    for pc in range(ENTRY, MAPLE_CALL+4, 2):
        replaced = bytearray(source)
        struct.pack_into('<H',replaced,pc-LOAD,0xffff)
        assert not valid(bytes(replaced)), hex(pc)
        # A damaged extended startup cannot fall back to the cached-lock driver.
        assert lib.kui_vmu_native_discover(bytes(replaced),len(source),LOAD,
                                          ct.create_string_buffer(104)) != 1,hex(pc)

    n = 0
    def refused(address,fmt,value):
        nonlocal n
        updated=bytearray(source)
        struct.pack_into(fmt,updated,address-LOAD,value)
        assert not valid(bytes(updated)), (hex(address),hex(value))
        n+=1

    # A real BSR in the low half of this frame pool's third function address
    # would enter the heap call's delay slot if the word were executed. Break
    # each part of its data ownership separately.
    for addr in (0x8c06915e,0x8c069160,0x8c069162,0x8c069164,0x8c069166,
                 0x8c06907c,0x8c06907e,0x8c06912c,0x8c06912e,0x8c06909a):
        refused(addr,'<H',0xffff)
    refused(0x8c069174,'<I',0)
    # Add real external branch, address-generation, and read-width conflicts
    # after replacing a neutral halfword outside the existing startup/pool.
    for target in (0x8c069160,0x8c069162,0x8c069170,0x8c069172,ENTRY+2,HEAP_CALL+2,
                   MAPLE_CALL+2):
        site=0x8c069058
        refused(site,'<H',0xb000|(((target-site-4)//2)&4095))
        if target%4==0:
            refused(site,'<H',0xc700|((target-(site+4))//4))
        for alias in (0,0x80000000,0xa0000000):
            refused(LOAD+0x1000,'<I',(target&0x1fffffff)|alias)
    site=0x8c069058
    refused(site,'<H',0x9000|((0x8c069170-site-4)//2))
    assert valid(source)
    print(f'Proved MDK2 deferred heap/Maple calls, original P2 write in three initial '
          f'states, {(MAPLE_CALL+4-ENTRY)//2} instruction mutations, {n} ownership '
          'controls and both wrong-order controls.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('startup',type=Path)
    source=parser.parse_args().startup.read_bytes()
    assert hashlib.sha256(source).hexdigest() == SHA
    original_instructions(source)
    with tempfile.TemporaryDirectory() as folder:
        check(source,host_library(folder))


if __name__=='__main__':
    main()
