#!/usr/bin/env python3
"""Compare the actual linked native SCI block checks with independent CRC16."""
from itertools import product
from pathlib import Path
import random
import runpy
import sys

ROOT = Path(__file__).resolve().parent
support = runpy.run_path(str(ROOT / 'toy51-linked-resume.py'))
Replay, cut, pio = support['Replay'], support['cut'], support['pio']
RESULT, TOKEN = support['RESULT'], support['TOKEN']
RDR = cut.R + 0x4a4
READY_LBA = cut.R + 0x47c
CASE_PATTERNS = [
    bytes(512), b'\xff'*512, b'\xa5\x5a'*256, bytes(range(256))*2,
    *(random.Random(seed).randbytes(512) for seed in range(4)),
]


def intact(image, area_number, body, wire, damage):
    cpu = Replay(image, area_number, 1, 0, False)
    high, low = pio.crc16(body).to_bytes(2, 'big')
    payload = bytearray(body)
    if damage == 'payload':
        payload[451] ^= 0x20
    elif damage == 'first_crc':
        high ^= 4
    elif damage == 'second_crc':
        low ^= 0x20
    to_wire = pio.reverse if wire else lambda byte: byte
    cpu.store_block(cpu.area, bytes(map(to_wire, payload + bytes([high]))))
    cpu.write(RDR + area_number, to_wire(low), 1)
    cpu.write(cut.AREA_READY + area_number, 1, 1)
    cpu.write(cut.AREA_WIRE + area_number, wire, 1)
    cpu.write(READY_LBA + area_number * 4, pio.LBA)
    cpu.write(cut.ARRIVED, pio.LBA+1)
    cpu.reg_trace.clear()
    count = cpu.call(pio.STREAM_TAKE, [pio.LBA, RESULT])
    assert cpu.read(RESULT) == (5 if damage else 0)
    assert count['result'] == (0 if damage else cpu.area)
    assert cpu.read(cut.AREA_READY + area_number, 1) == 0
    assert not cpu.reg_trace
    assert cpu.block(cpu.area, 512) == payload
    return count['instructions']


def recovered(image, area_number, cut_index, held):
    body = support['BODY']
    cpu = Replay(image, area_number, cut_index, held, True)
    cpu.parked(True)
    cpu.suffix(2)
    high, low = pio.crc16(body).to_bytes(2, 'big')
    cpu.store_block(cpu.area+cpu.lost+1, cpu.wire_body[cpu.lost+1:]+bytes([pio.reverse(high)]))
    cpu.hardware.update({cut.TCR:0,cut.DAR:(cpu.area&0x1fffffff)+513,
                         cut.CHCR:0x4913,pio.RDR:pio.reverse(low),pio.SSR:0xe4})
    assert cpu.call(cut.POLL,[1])['result'] == 0
    cpu.reg_trace.clear()
    count = cpu.call(pio.STREAM_TAKE, [pio.LBA, RESULT])
    assert cpu.read(RESULT) == 0 and count['result'] == cpu.area
    assert cpu.block(cpu.area, 512) == body
    assert cpu.read(support['COUNT']) == 1 and cpu.read(support['KEPT'],1) == 0
    assert not cpu.reg_trace
    return count['instructions']


def main(base, candidate):
    checked = dict(wire=[], pio=[], recovered=[])
    for area, body, wire, damage in product(range(2), CASE_PATTERNS, (False, True),
                                           (None,'payload','first_crc','second_crc')):
        instructions = [intact(image, area, body, wire, damage) for image in (base,candidate)]
        assert instructions[0] > instructions[1], instructions
        checked['wire' if wire else 'pio'].append(instructions)
    for area, at, held in product(range(2), (1,43,97,127,255,318,353,509), (0,1)):
        instructions = [recovered(image, area, at, held) for image in (base,candidate)]
        assert instructions[0] > instructions[1], instructions
        checked['recovered'].append(instructions)
    for key, values in checked.items():
        print(f'{key}: {len(values)} matched cases, instructions '
              f'{min(a for a,_ in values)}–{max(a for a,_ in values)} → '
              f'{min(b for _,b in values)}–{max(b for _,b in values)}, '
              f'{min(a-b for a,b in values)}–{max(a-b for a,b in values)} fewer')


if __name__ == '__main__':
    main(*(Path(arg) for arg in sys.argv[1:]))
