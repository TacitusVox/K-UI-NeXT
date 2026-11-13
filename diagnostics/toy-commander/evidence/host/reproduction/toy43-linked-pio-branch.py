#!/usr/bin/env python3
"""Replay the shipped SH-4 resident's fast SCI receive from a parked CMD18."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / 'trace-reader' / 'tools'))
from profile_vmu_native_release_sh import ReleaseSH, MASK, signed

SMR, BRR, SCR, TDR, SSR, RDR = range(0xffe00000, 0xffe00018, 4)
PCTR, PDTR = 0xff80002c, 0xff800030
REGION, CARD = 0x8c420100, 0x8c440000
AREA0, AREA1 = REGION + 0x20, REGION + 0x540
BYTE_COUNT = 514
LBA = 0x60abc
REGION_STORAGE, PORT_FAULT = '_kui_retail_async_region_storage', REGION + 0x4f4
STREAM_FETCH, STREAM_TAKE = '_kui_sci_stream_fetch', '_kui_sci_stream_take'


def reverse(byte):
    return int(f'{byte:08b}'[::-1], 2)


def crc16(payload):
    crc = 0
    for byte in payload:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xffff
    return crc


class LinkedPIO(ReleaseSH):
    def __init__(self, image, *, fault=0, corrupt=False):
        self.hardware = {SMR: 0x80, BRR: 0, SCR: 0x30, TDR: 0xff,
                         SSR: 0x84, RDR: 0, PCTR: 0x4000, PDTR: 0x80}
        self.sent = 0
        self.read_count = 0
        self.saw_tdre = self.saw_rdrf = False
        self.pending = 0
        self.fault = fault
        body = bytes((11 + i * 37) & 255 for i in range(512))
        self.payload = body + (crc16(body) ^ corrupt).to_bytes(2, 'big')
        self.wire = [0xfe, *self.payload, 0xff, 0xfe, *self.payload]
        super().__init__(image)
        symbols = self.symbols
        self.write(symbols[REGION_STORAGE][0], REGION)
        self.write(REGION + 0x464, CARD)
        self.write(REGION + 0x468, AREA0)
        self.write(REGION + 0x46c, AREA1)
        self.write(REGION + 0x470, 1)  # PAUSED
        self.write(REGION + 0x474, LBA)
        self.write(REGION + 0x4ad, 3, 1)  # exact owner and movie active
        self.write(REGION + 0x4f3, 1, 1)  # acquired
        self.write(CARD + 12, symbols['_select_card'][0])
        self.write(CARD + 16, symbols['_transfer'][0])
        self.store_block(AREA0, bytes([0xaa]) * 544)
        self.store_block(AREA1, bytes([0xaa]) * 544)

    def read(self, address, count=4):
        if address in self.hardware:
            if address == SSR:
                if self.pending:
                    self.hardware[SSR] |= 0x80
                    self.pending -= 1
                    if not self.pending:
                        self.hardware[SSR] |= 4
                        if self.sent == self.fault:
                            self.hardware[SSR] |= 0x20
                        else:
                            assert not self.hardware[SSR] & 0x40
                            self.hardware[SSR] |= 0x40
                            self.hardware[RDR] = reverse(self.wire[self.sent - 1])
                self.saw_tdre |= bool(self.hardware[SSR] & 0x80)
                self.saw_rdrf |= bool(self.hardware[SSR] & 0x40)
            elif address == RDR:
                assert self.saw_rdrf and self.hardware[SSR] & 0x40
                assert self.read_count + 1 == self.sent and not self.pending
                self.read_count += 1
            return self.hardware[address]
        assert address < 0xf0000000, (f'{address:x}', f'{self.executing:x}')
        return super().read(address, count)

    def write(self, address, value, count=4):
        if address in self.hardware:
            value &= (1 << (count * 8)) - 1
            if address == SSR:
                launch = bool(self.hardware[SCR] & 0x20 and self.hardware[SSR] & 0x80 and not value & 0x80)
                if self.hardware[SSR] & 0x40 and not value & 0x40:
                    assert self.saw_rdrf and self.sent == self.read_count
                    self.saw_rdrf = False
                self.hardware[SSR] = self.hardware[SSR] & (value | 7)
                if launch:
                    assert self.saw_tdre and self.hardware[TDR] == 0xff and self.hardware[SCR] == 0x30
                    assert not self.hardware[PDTR] & 0x80 and not self.pending
                    assert self.sent == self.read_count and not self.hardware[SSR] & 0x40
                    self.sent += 1
                    self.saw_tdre = False
                    self.pending = 4 + self.sent % 5
                    self.hardware[SSR] &= ~4
            else:
                self.hardware[address] = value
            return
        assert address < 0xf0000000, (f'{address:x}', f'{self.executing:x}')
        super().write(address, value, count)

    def plain(self, op, pc):
        if op & 0xf0ff == 0x4018:  # shll8
            n = op >> 8 & 15
            self.r[n] = (self.r[n] << 8) & MASK
        elif op & 0xf0ff == 0x4019:  # shlr8
            n = op >> 8 & 15
            self.r[n] >>= 8
        elif op & 0xff00 == 0x8400:
            address = (self.r[op >> 4 & 15] + (op & 15)) & MASK
            self.r[0] = signed(self.read(address, 1), 8) & MASK
        elif op & 0xf00f == 0x0004:
            n, m = op >> 8 & 15, op >> 4 & 15
            self.write((self.r[0]+self.r[n]) & MASK, self.r[m], 1)
        elif op & 0xf00f == 0x0006:
            n, m = op >> 8 & 15, op >> 4 & 15
            self.write((self.r[0]+self.r[n]) & MASK, self.r[m])
        elif op & 0xf00f == 0x000c:
            n, m = op >> 8 & 15, op >> 4 & 15
            self.r[n] = signed(self.read((self.r[0]+self.r[m]) & MASK, 1), 8) & MASK
        elif op & 0xf0ff == 0x4011:
            self.flag(self.r[op >> 8 & 15] >= 0x80000000)
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1


def main():
    path = Path(sys.argv[1])
    expected = LinkedPIO(path)
    check_at = REGION + 0xf00
    for i, area in enumerate((AREA1, AREA0)):
        fetch = expected.call(STREAM_FETCH, (LBA+i, 65536, 0))
        assert fetch['result'] == 0
        assert expected.block(area, 544) == expected.payload + bytes([0xaa]) * 30
        taken = expected.call(STREAM_TAKE, (LBA+i, check_at))
        assert taken['result'] == area and expected.read(check_at) == 0
    assert expected.sent == expected.read_count == 1031
    assert expected.hardware[PDTR] & 0x80 and expected.hardware[SCR] == 0x30
    corrupt = LinkedPIO(path, corrupt=True)
    assert corrupt.call(STREAM_FETCH, (LBA, 65536, 0))['result'] == 0
    assert corrupt.call(STREAM_TAKE, (LBA, check_at))['result'] == 0
    assert corrupt.read(check_at) == 5
    fault = LinkedPIO(path, fault=78)
    assert fault.call(STREAM_FETCH, (LBA, 65536, 0))['result'] == 7
    assert fault.read(PORT_FAULT, 1) == 1 and fault.read_count == 77
    assert fault.call(STREAM_TAKE, (LBA, check_at))['result'] == 0
    print('Exact linked Test 43 low SH-4: two consecutive 514-byte PIO blocks, CRC failure, hardware overrun, ABI preservation')


if __name__ == '__main__':
    main()
