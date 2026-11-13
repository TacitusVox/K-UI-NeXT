#!/usr/bin/env python3
"""Replay exact Mode 59 copies with the linked low resident mapper."""

import argparse
from pathlib import Path
from random import Random
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "trace-reader/tools"))
from profile_vmu_native_release_sh import ReleaseSH
from check_vmu_native_worker_gateway import read_elf

SRC = 0x8c600000
DST = 0xac700000
REGION = 0x8c200000
CURSOR = 0x8c220000
SERVICE = REGION + 0x320
ENGINE_DEST = REGION + 0x27c + 56
LEASE = 0x0c740000


class RefusedMap(Exception):
    pass


class CopyCPU(ReleaseSH):
    def __init__(self, aux, resident=None):
        super().__init__(aux)
        self.purged = []
        self.mapper = None
        self.low_control = None
        self.guest_writes = []
        if resident:
            data, _, segments, _, symbols = read_elf(resident)
            for kind, offset, address, _, stored, memory, _, _ in segments:
                if kind == 1:
                    self.store_block(address, data[offset:offset + stored] + bytes(memory - stored))
            self.mapper = symbols["_native_background_map"][0]
            self.low_control = symbols["_kui_vmu_observe_boot_control"][0]
            self.failure = symbols["_kui_vmu_observe_low_failure"][0]
            self.region_storage = symbols["_kui_retail_async_region_storage"][0]
            self.symbols["_write_out"] = symbols["_write_out"]

    def read(self, address, count=4):
        if self.mapper and address == self.failure:
            raise RefusedMap
        return super().read(address, count)

    def write(self, address, value, count=4):
        if getattr(self, "active", False) and 0x0c600000 <= (address & 0x1fffffff) < 0x0d000000 and \
                not 0x0c7eff00 <= (address & 0x1fffffff) < 0x0c7f0000:
            self.guest_writes.append((address, count))
        super().write(address, value, count)

    def plain(self, op, pc):
        if op & 0xf0ff == 0x00a3:  # ocbp @Rn
            self.purged.append(self.r[(op >> 8) & 15])
        elif op & 0xff00 == 0x8400:  # mov.b @(disp,Rm),R0
            value = self.read(self.r[(op >> 4) & 15] + (op & 15), 1)
            self.r[0] = value | (0xffffff00 if value & 128 else 0)
        else:
            return super().plain(op, pc)
        self.executing = pc
        self.instructions += 1

    def publish(self, count, address, staged, *, destination=None, command=17, size=None,
                status=4, control_lease=LEASE, map_override=None, per_sector=False):
        self.store_block(SRC, staged)
        initialized = min(count * 2048 + 32, 0x0d000000 - (address & 0x1fffffff))
        self.store_block(address, b"!" * initialized)
        self.write(ENGINE_DEST, address)
        self.write(SERVICE + 4, self.mapper if map_override is None else map_override)
        self.write(SERVICE + 48, command)
        self.write(SERVICE + 60, (address & 0x1fffffff) if destination is None else destination)
        self.write(SERVICE + 68, count * 2048 if size is None else size)
        self.write(CURSOR + 16, count)
        self.write(self.low_control + 8, status)
        self.write(self.low_control + 12, control_lease)
        self.write(self.low_control + 32, (control_lease | 0xa0000000) + 0x12000)
        if per_sector:
            # The original writer resolves the active engine through its
            # private low storage cell, as it does from the baseline aux.
            self.write(self.region_storage, REGION)
        self.purged.clear()
        self.guest_writes.clear()
        if per_sector:
            return {"result": 1, "instructions": sum(self.call("_write_out",
                        [0, index * 2048, SRC + index * 2352 + 16, 2048])["instructions"]
                        for index in range(count))}
        return self.call("_toy_resume_fast_publish", [REGION, CURSOR, SRC])


def check(aux, old, resident, old_resident):
    random = Random(59)
    modern, baseline = CopyCPU(aux, resident), CopyCPU(old, old_resident)
    sample = random.randbytes(4096)
    cases = 0
    results = {}
    for length in (0, 1, 2, 3, 15, 16, 31, 32, 33, 63, 64, 65, 127, 128,
                   144, 252, 512, 2048):
        for src in range(4):
            for dst in range(4):
                args = [DST + 4 + dst, SRC + src, length]
                for cpu, name in ((modern, "_toy_resume_copy"), (baseline, "_memcpy")):
                    cpu.store_block(SRC, sample)
                    cpu.store_block(DST, b"!" * 4096)
                    stats = cpu.call(name, args)
                    expected = b"!" * (4 + dst) + sample[src:src + length]
                    expected += b"!" * (4096 - len(expected))
                    assert cpu.block(DST, 4096) == expected
                    if src == dst == 0 and length in (512, 2048):
                        results[name, length] = stats["instructions"]
                cases += 1
    for count in (1, 14, 64):
        raw = random.randbytes(count * 2352)
        address = DST & ~0x20000000
        for alias in (0, 0x8c000000, 0xac000000):
            report = modern.publish(count, address, raw, destination=(address & 0xffffff) | alias)
            assert report["result"] == 1
            expected = b"".join(raw[i * 2352 + 16:i * 2352 + 2064] for i in range(count))
            assert modern.block(DST, count * 2048 + 32) == expected + b"!" * 32
            assert modern.purged == list(range(address, address + count * 2048, 32))
            assert modern.guest_writes == [(value, 4) for value in range(DST, DST + count * 2048, 4)]
            if count == 14:
                results["publish14", 2048] = report["instructions"]
            cases += 1
        if old_resident and count == 14:
            report = baseline.publish(count, address, raw, per_sector=True)
            assert baseline.block(DST, count * 2048 + 32) == expected + b"!" * 32
            assert baseline.purged == modern.purged and baseline.guest_writes == modern.guest_writes
            results["publish14 baseline", 2048] = report["instructions"]
            cases += 1
    raw = random.randbytes(2352)
    for address, kwargs in (
        ((DST & ~0x20000000) + 2, {}),
        (DST & ~0x20000000, {"destination": 0}),
        (DST & ~0x20000000, {"command": 18}),
        (DST & ~0x20000000, {"size": 32}),
        (DST & ~0x20000000, {"map_override": 0}),
        (DST & ~0x20000000, {"map_override": 0x8c010000}),
    ):
        # Last case uses an unsupported mapper returning a cached alias;
        # the fast helper must decline without writing the payload.
        if kwargs.get("map_override") == 0x8c010000:
            modern.stubs[kwargs["map_override"]] = DST & ~0x20000000
        result = modern.publish(1, address, raw, **kwargs)
        assert result["result"] == 0 and not modern.guest_writes
        assert modern.block(address, 2080) == b"!" * 2080
        cases += 1
    for address, status in ((0x8c740000, 4), (0x8cfffffc, 4), (0x8c700000, 5)):
        try:
            modern.publish(1, address, raw, status=status)
        except RefusedMap:
            assert not modern.guest_writes and not modern.purged
        else:
            raise AssertionError("linked native mapper admitted protected or invalid RAM")
        cases += 1
    for length in (512, 2048):
        old_count, new_count = results["_memcpy", length], results["_toy_resume_copy", length]
        assert new_count * 2 < old_count
        print(f"{length} aligned bytes: {old_count} -> {new_count} SH-4 instructions")
    prior = f"{results['publish14 baseline', 2048]} -> " if old_resident else ""
    print(f"{cases} linked copy/mapper cases, 14-sector full publish {prior}{results['publish14', 2048]} instructions")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("resident", type=Path)
    parser.add_argument("--old-resident", type=Path)
    args = parser.parse_args()
    check(args.candidate, args.baseline, args.resident, args.old_resident)
