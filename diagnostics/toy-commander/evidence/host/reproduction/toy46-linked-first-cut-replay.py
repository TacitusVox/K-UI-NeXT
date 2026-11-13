#!/usr/bin/env python3
"""Exercise actual linked SH-4 SCI poll/finish with modeled peripheral registers."""

import importlib.util
from itertools import product
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("linked_pio", ROOT / "toy43-linked-pio-branch.py")
pio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pio)

SCMR, SPTR, STBCR = 0xFFE00018, 0xFFE0001C, 0xFFC00004
SAR, DAR, TCR, CHCR, DMAOR = 0xFFA00010, 0xFFA00014, 0xFFA00018, 0xFFA0001C, 0xFFA00040
POLL = "_kui_sci_stream_poll"
R = pio.REGION
STATE, POSITION, FILL, ARRIVED = (R + n for n in (0x470, 0x474, 0x478, 0x498))
AREA_READY, AREA_WIRE, ACTIVE, RETRY = (R + n for n in (0x4A0, 0x4A2, 0x4AD, 0x4AF))
OVERRUN, FOREIGN, RETRY_ORER = (R + n for n in (0x4C0, 0x4CC, 0x4D0))
SAVED = (0x01234567, 0x00123456, 0x00012345, 0x00005440)
DATA = bytes((19 + i * 13) & 255 for i in range(544))


class Replay(pio.LinkedPIO):
    def __init__(self, image, *, selected, arm, area, received, slow, later, reason, held):
        super().__init__(image)
        self.area = (pio.AREA0, pio.AREA1)[area]
        self.held_reads = 0
        self.reg_trace = []
        self.later = later
        self.hardware.update({
            SCMR: 0, SPTR: 0x83, STBCR: 0, SAR: 0x1FE00014,
            DAR: (self.area & 0x1fffffff) + received, TCR: 513 - received,
            CHCR: 0x4911, DMAOR: 0x8201, pio.SSR: 0x84,
            pio.SCR: 0x50, pio.PDTR: 0, pio.BRR: int(slow), pio.RDR: 0x2e,
        })
        if reason in ("overrun", "foreign", "bad_reset"):
            self.hardware[pio.SSR] |= 0x20
        if held:
            self.hardware[pio.SSR] |= 0x40
        if reason == "no_overrun":
            self.hardware[CHCR] &= ~1
        if reason == "complete":
            self.hardware[CHCR] |= 2
        if reason == "foreign":
            self.hardware[SAR] = 0x1FE00018
        if reason == "bad_reset":
            self.hardware[SPTR] = 0x80
        self.write(STATE, 2)
        self.write(FILL, area)
        self.write(ACTIVE, 7 if selected else 3, 1)
        self.write(RETRY, arm, 1)
        self.write(AREA_WIRE + area, 3 if slow else 1, 1)
        for index, value in enumerate(SAVED):
            self.write(R + 0x488 + index * 4, value)
        self.store_block(self.area, DATA)

    def read(self, address, count=4):
        if address in self.hardware:
            self.reg_trace.append(("read", address, count))
            if address == pio.RDR:
                assert self.hardware[pio.SSR] & 0x40
                self.held_reads += 1
            return self.hardware[address]
        return super().read(address, count)

    def write(self, address, value, count=4):
        if address in self.hardware:
            value &= (1 << (count * 8)) - 1
            self.reg_trace.append(("write", address, value, count))
            self.hardware[address] = value
            if address == STBCR and value & 1:
                self.hardware.update({pio.SCR: 0, pio.SMR: 0, pio.BRR: 0xff,
                                      SCMR: 0, pio.SSR: 0x84})
            return
        super().write(address, value, count)


def case(path, *, selected=True, arm=2, area=0, received=212, slow=False,
         later=False, reason="overrun", held=False, expected=253):
    cpu = Replay(path, selected=selected, arm=arm, area=area, received=received,
                 slow=slow, later=later, reason=reason, held=held)
    result = cpu.call(POLL, [int(later)])
    returned = {"overrun": 6, "no_overrun": 6, "foreign": 2, "bad_reset": 7,
                "complete": 0, "busy": 1}[reason]
    assert result["result"] == returned, (reason, result)
    assert cpu.read(RETRY, 1) == expected, (selected, arm, slow, reason, cpu.read(RETRY, 1))
    assert cpu.block(cpu.area, len(DATA)) == DATA
    assert cpu.read(POSITION) == pio.LBA + (reason == "complete")
    assert cpu.read(AREA_READY + area, 1) == (reason == "complete")
    assert cpu.read(ARRIVED) == (pio.LBA + 1 if reason == "complete" else 0)
    assert cpu.read(STATE) == {"complete": 1, "busy": 2}.get(reason, 3)
    if reason == "busy":
        assert all(operation[0] == "read" for operation in cpu.reg_trace)
        assert cpu.held_reads == 0
    else:
        assert cpu.held_reads == int(held)
        assert cpu.hardware[pio.PDTR] & 0x80
        if reason != "bad_reset":
            assert cpu.hardware[pio.SCR] == 0x30 and cpu.hardware[pio.BRR] == 0
    if reason != "foreign" and reason != "busy":
        assert tuple(cpu.hardware[register] for register in (SAR, DAR, TCR, CHCR)) == SAVED
    if reason == "foreign":
        assert all(operation[0] != "write" or operation[1] not in (SAR, DAR, TCR, CHCR)
                   for operation in cpu.reg_trace)
    assert cpu.read(OVERRUN) == int(reason in ("overrun", "no_overrun"))
    assert cpu.read(RETRY_ORER) == int(reason == "overrun" and arm == 2)
    assert not any(operation[0] == "write" and operation[1] in (DMAOR, pio.TDR)
                   for operation in cpu.reg_trace)
    return result["instructions"]


def main():
    for filename in sys.argv[1:]:
        path = Path(filename)
        cases = []
        for selected, area, held, later in product((False, True), repeat=4):
            for received in (1, 212, 511):
                cases.append(case(path, selected=selected, area=int(area), held=held,
                                  later=later, received=received, expected=253 if selected else 1))
        cases.append(case(path, arm=0, expected=0))
        cases.append(case(path, arm=1, expected=0))
        cases.append(case(path, arm=254, expected=0))
        cases.append(case(path, selected=False, slow=True, expected=253))
        cases.append(case(path, selected=True, slow=True, expected=253))
        cases.append(case(path, reason="no_overrun", expected=0))
        cases.append(case(path, reason="foreign", expected=0))
        cases.append(case(path, reason="bad_reset", expected=0))
        cases.append(case(path, reason="complete", received=513, held=True, expected=0))
        cases.append(case(path, reason="busy", expected=2))
        print(f"{path}: {len(cases)} linked SH SCI poll/finish cases, {sum(cases)} instructions")


if __name__ == "__main__":
    main()
