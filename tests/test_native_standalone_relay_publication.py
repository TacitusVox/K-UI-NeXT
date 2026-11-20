#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Feed the actual SH resume stub's CCR into its linked standalone publisher."""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_native_standalone_reader import StageStop, relay_publication_ccr
from check_vmu_native_worker_gateway import read_elf
from test_native_standalone_early_irq import EarlyStageCPU, Family, LOAD, production_family_library


def require(ok, message):
    if not ok:
        raise ValueError(message)


def publisher_case(stage, resident, library, owner, real_source, ccr, *, sr=0x400000f0, corrupt=None):
    cpu = EarlyStageCPU(stage, resident, owner, library)
    roles = [owner['vector'], *owner['commons'], owner['default']]
    if real_source:
        # Both C callers execute on the SH. The pure family finder uses the
        # production C on the corresponding complete executable in model RAM.
        cpu.call('_standalone_source_check')
        cpu.call('_standalone_source_check')
        require(len(cpu.family_calls) == 2 and all(case == dict(result=1, roles=roles)
                for case in cpu.family_calls), 'fresh SDK proofs failed')
    else:
        cpu.put_block(stage[4]['_standalone_family'][0], struct.pack('<4I', *roles))
        cpu.put(stage[4]['_standalone_family_found'][0], 1, 1)
    cpu.ccr = ccr
    cpu.sr = sr
    if corrupt:
        corrupt(cpu)
    refused = None
    try:
        cpu.call('_standalone_source_publish')
    except StageStop as stop:
        _, address, detail = stop.args
        message = bytes(cpu.read(address+i, 1) for i in range(96)).split(b'\0', 1)[0].decode()
        refused = (message, detail)
    retained = [(a, v, n) for _, a, v, n in cpu.writes
                if 0 <= (a & ~0x20000000) - cpu.region < 0x760]
    if refused:
        require(not retained, 'failed admission wrote into the retained SDK state')
    else:
        out = cpu.region | 0x20000000
        expected = [(out+offset, value, 4) for offset, value in zip(
                    (0, 4, 8, 12, 0x534, 16), roles+[roles[-1], 0x53524431])]
        require(retained == expected, 'wrong source publication addresses/order')
        require(not cpu.read(out+0x530) and not cpu.read(out+0x538), 'publication armed the live route')
        require(not cpu.ccr_writes and not cpu.vbr_writes and not cpu.iprb_writes,
                'publication changed hardware state')
    return refused


def run(build, library, sources, historical=False):
    stage = read_elf(build/'stage.elf')
    resident = read_elf(build/'resident-sci.elf')
    ccr = relay_publication_ccr(stage)
    require(ccr == 0x100, 'actual relay did not select the instruction-only state')
    roles = [LOAD+0x100, LOAD+0x134, LOAD+0x238, LOAD+0x400]
    fixture = dict(image=bytes(0x900), vector=roles[0], commons=roles[1:3], default=roles[3], vbr=0x8c00f400)
    case = lambda c, **kw: publisher_case(stage, resident, library, fixture, False, c, **kw)
    valid = case(ccr)
    old_stop = ('STANDALONE SOURCE PUBLICATION REFUSED', 1)
    require(valid == (old_stop if historical else None), 'real relay/publisher result differs: '+str(valid))
    require(case(0) is None, 'legacy cache-off publication failed')
    if not historical:
        for unsafe in (1, 0x101, 0x121, 0x908):
            require(case(unsafe) == ('STANDALONE PUBLISH CACHE STATE', unsafe), 'invalid CCR accepted or misreported')
        for unsafe in (0xf0, 0x400000e0):
            require(case(ccr, sr=unsafe) == ('STANDALONE PUBLISH CPU STATE', unsafe), 'invalid SR accepted or misreported')
        controls = [
            lambda c: c.put(stage[4]['_standalone_family_found'][0], 0, 1),
            lambda c: c.put(stage[4]['_resident_limit'][0], 0),
            lambda c: c.put(resident[4]['_kui_retail_async_region_storage'][0], c.region+32),
            lambda c: c.put(c.region+16, 1),
            lambda c: c.put(c.region+0x530, 1),
        ]
        for mutate in controls:
            require(case(ccr, corrupt=mutate) is not None, 'damaged SDK ownership admitted')
    result = []
    for path in sources:
        data = path.read_bytes()
        family = Family()
        require(library.kui_native_sdk_sci_family_find(ctypes.create_string_buffer(data), len(data),
                LOAD, ctypes.byref(family)) == 1, 'source has no unique SDK SCI family: '+str(path))
        vector, normal, fpu, default = struct.unpack('<4I', bytes(family))
        owner = dict(image=data, vector=vector, commons=[normal, fpu], default=default, vbr=0x8c00f400)
        refused = publisher_case(stage, resident, library, owner, True, ccr)
        require(refused == (old_stop if historical else None), 'unexpected real-executable stop: '+str(refused))
        result.append(dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), refused=refused))
    return dict(live_relay_ccr=ccr, historical=historical, real_sources=result)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('build', type=Path)
    p.add_argument('--original-build', type=Path)
    p.add_argument('--executable', type=Path, action='append', default=[])
    args = p.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        lib = production_family_library(tmp)
        if args.original_build:
            print(json.dumps(run(args.original_build, lib, args.executable, True)))
        print(json.dumps(run(args.build, lib, args.executable)))
