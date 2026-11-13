#!/usr/bin/env python3
"""Run the actual linked Toy read-ahead binder on modeled cold memory.

Models bounded cold memory helpers, relocator, publisher and worker stack
gateway using the existing read-ahead stage proof. The relocated initializer,
guest probe writes, cache-line publication and readbacks execute as SH.
"""
import argparse
from hashlib import sha256
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "trace-reader" / "tools"))
import check_vmu_native_readahead_stage as base
import check_vmu_native_sdk_sci_route as sdk
from check_vmu_native_worker_gateway import read_elf
from profile_vmu_native_release_sh import ReleaseSH
from runtime_package import ADDRESS, flatten_elf, verify
from check_loader_layout import inspect_elf
from sh4_delay_slots import validate_delay_slot
sys.path.insert(0, str(ROOT / "toy52-53-proof"))
from edc52 import reference_table

MOVIE_LITERAL, MOVIE_ORIGINAL = 0x8C06D428, 0x8C08D638
CHUNK_LITERAL, CHUNK_ORIGINAL = 0x8C08D950, 0x8C08EDBC
HOOKS = {MOVIE_LITERAL: MOVIE_ORIGINAL, CHUNK_LITERAL: CHUNK_ORIGINAL}
CONFIG_BYTES = 88
EDC_BYTES = 4096
EXPECTED_EDC_TABLE = reference_table()


def toy_layout(aux):
    size = aux[4]["_kui_native_readahead_aux_context"][1]
    assert size == 1584
    config = 1184
    return dict(diagnostic=False, pio=False, pair=False, public_bytes=config,
                context_bytes=size, config=config, cache=config + CONFIG_BYTES,
                initialized=config + CONFIG_BYTES + 20)


class ToyBinder(base.ReadAheadBinderCPU):
    def __init__(self, stage, resident, aux, original, mode, payload, cache, exact):
        sdk.OWNER_CONSTANTS["Toy"] = dict(bytes=len(original),
            digest=sha256(original).hexdigest(), default=0x8C086F8E)
        sdk.GUARD = resident[4]["__retail_hook_stack_bottom"][0]
        base.auxiliary_layout = toy_layout
        super().__init__(stage, resident, aux, owner_name="Toy", payload=payload, cache=cache)
        self.mode, self.exact = mode, exact
        if not exact:
            self.put_bytes(stage[4]["_native_driver"][0]+72, bytes(32))
        self.blob = stage[4]["__retail_resident_sci_blob_start"][0]
        last = stage[4]["__retail_resident_sci_blob_end"][0]
        count = self.low_end - 0x8C004000
        assert last - self.blob == count
        assert base.initialized_at(stage, self.blob, count) == inspect_elf(resident[0], 0x8C004000, sdk.GUARD)["payload"]
        self.put32(stage[4]["_resident_blob"][0], self.blob)
        for addr, word in HOOKS.items():
            at = addr - 0x8C010000
            assert int.from_bytes(original[at:at + 4], "little") == word
            self.put_bytes(addr, original[at:at + 4])
        self.put32(stage[4]["_toy_suite_source"][0], 1)
        self.ranges = (0x69000, 100, 0x69100, 3000, 0x6C000, 100, 0x6E000, 25000)
        for index, word in enumerate(self.ranges):
            self.put32(stage[4]["_toy_movie_ranges"][0] + 4 * index, word)

    def plain(self, op, pc):
        n, m = (op >> 8) & 15, (op >> 4) & 15
        if op >> 8 == 0xCA:  # xor #imm,R0
            self.r[0] ^= op & 255
            return
        if op & 0xF00F == 0x6007:  # not Rm,Rn
            self.r[n] = ~self.r[m] & base.MASK
            return
        if op & 0xF00F == 0x300A:  # subc Rm,Rn
            rhs, lhs = self.r[m] + (self.sr & 1), self.r[n]
            self.r[n] = (lhs - rhs) & base.MASK
            self.flag(lhs < rhs)
            return
        if op & 0xF0FF == 0x4019:  # shlr8 Rn
            self.r[n] >>= 8
            return
        return super().plain(op, pc)

    def _initialize(self):
        self.init_calls += 1
        entry, stack, config, unused = self.r[4:8]
        header = [self.read(self.dest + 4 * index) for index in range(16)]
        assert entry == header[4] and stack == self.stack_top - 32
        assert self.r[15] <= config <= self.r[15] + 256 - CONFIG_BYTES
        assert self.r[15] <= unused <= self.r[15] + 256 - 4
        values = [1, CONFIG_BYTES, self.region, self.descriptor, self.data, base.DATA_BYTES,
                  self.payload & 0x1FFFFFFF, self.geometry.LEASE_BYTES,
                  *self.ranges, 0, 0, 0, 0, 1, self.exact]
        assert [self.read(config + 4 * index) for index in range(len(values))] == values
        context = header[5]
        assert len(self.publisher_calls) == 1
        for offset in range(4, self.layout["public_bytes"]):
            self.write(context + offset, 0xA5, 1)
        if self.mode == 52:
            table = (self.data & ~0x20000000) + base.DATA_BYTES - EDC_BYTES
            assert not table & 31
            self.put_bytes(table-32, b'\xa5' * (EDC_BYTES + 64))
            writes = len(self.writes)
        before, owner_sp = self.snapshot(), self.r[15]
        self.r[15], self.r[4], self.r[5], self.pr = stack, config, unused, base.RETURN + 4
        self.run_to(entry, base.RETURN + 4)
        assert self.r[0] == 0
        if self.mode == 52:
            expected = EXPECTED_EDC_TABLE if self.exact else b'\xa5' * EDC_BYTES
            assert bytes(self.read(table-32+i, 1) for i in range(EDC_BYTES + 64)) == (
                b'\xa5' * 32 + expected + b'\xa5' * 32)
            table_writes = [(address, count) for _, address, _, count in self.writes[writes:]
                            if table <= address & ~0x20000000 < table + EDC_BYTES]
            assert table_writes == ([(address, 4) for address in range(table, table+EDC_BYTES, 4)]
                                    if self.exact else [])
        assert self.r[8:15] == before[0][8:15] and self.r[15] == stack
        assert self.other == before[1] and self.sr & ~1 == before[3] & ~1
        assert self.control == before[10]
        self.r[15] = owner_sp
        offset = self.layout["config"]
        assert [self.read(context + offset + 4 * index) for index in range(len(values))] == values
        assert self.read(context + self.layout["cache"]) == self.data
        assert self.read(context + self.layout["cache"] + 12) == 0
        assert self.read(context + self.layout["initialized"]) == 1
        assert self.read(context) == 0
        return 0

    def callee(self):
        name = self.api[self.pc]
        if name not in ("_memcpy", "_memcmp", "_memset", "_retail_display_line"):
            return super().callee()
        self.apis.append(name)
        continuation = self.pr
        left, right, count = self.r[4:7]
        if name == "_memcpy":
            assert count in (4, 16, 32, 52), count
            data = bytes(self.read(right + i, 1) for i in range(count))
            for i, byte in enumerate(data):
                self.write(left + i, byte, 1)
            result = left
        elif name == "_memcmp":
            assert count in (32, 52), count
            result = int(any(self.read(left + i, 1) != self.read(right + i, 1) for i in range(count)))
        elif name == "_memset":
            assert right == 0
            assert (left == self.dest + len(self.aux_binary) and count == base.SPAN - len(self.aux_binary) or
                    self.r[15] <= left <= left + count <= self.r[15] + 256), (
                        hex(left), count, hex(self.r[15]))
            for i in range(count):
                self.write(left + i, 0, 1)
            result = left
        else:
            display = bytes(self.read(left + i, 1) for i in range(128)).split(b"\0", 1)[0]
            self.display_lines.append(display)
            result = 0
        for i in range(8):
            self.r[i] = (0x913579BD + i * 0x13579BD + len(self.apis)) & base.MASK
        self.r[0], self.sr, self.pr, self.pc = result, self.sr ^ 1, self.pr ^ 0x13572468, continuation

    def check(self):
        self.run_symbol("_native_readahead_bind")
        sdk.assert_c_abi(self, self.initial)
        assert self.init_calls == 1
        assert self.display_lines[0] == b"COLD 42 READ AHEAD BIND BEGIN"
        assert self.display_lines[-1] == b"COLD 45 READ AHEAD BOUND"
        positive,negative = {
            42:(b"SCI ORIGINAL GRAPHICS PRIORITY READY",b"SCI PRIORITY OWNER UNVERIFIED"),
            43:(b"SCI CONTINUOUS PIO READY",b"SCI PIO OWNER UNVERIFIED"),
            44:(b"SCI SEQUENTIAL PIO READY",b"SCI PIO OWNER UNVERIFIED"),
            45:(b"SCI BOUNDED DMA READY",b"SCI DMA OWNER UNVERIFIED"),
            46:(b"SCI FIRST-CUT PIO READY",b"SCI PIO OWNER UNVERIFIED"),
            48:(b"SCI VERIFIED STREAM RESUME READY",b"SCI RESUME OWNER UNVERIFIED"),
            49:(b"SCI RESUME CADENCE READY",b"SCI RESUME OWNER UNVERIFIED"),
            51:(b"SCI CONTROLLER RESUME READY",b"SCI RESUME OWNER UNVERIFIED"),
            52:(b"SCI FASTER EDC READY",b"SCI RESUME OWNER UNVERIFIED"),
            53:(b"SCI FASTER CRC16 READY",b"SCI RESUME OWNER UNVERIFIED"),
            54:(b"SCI INTRO BUFFER READY",b"SCI RESUME OWNER UNVERIFIED"),
        }[self.mode]
        assert self.display_lines[-2] == (positive if self.exact else negative), self.display_lines
        entry, context = self.read(self.dest + 12), self.read(self.dest + 20)
        assert self.read((self.region + 0x500) | 0x20000000) == entry
        assert self.read((self.region + 0x504) | 0x20000000) == context
        for index, addr in enumerate(HOOKS):
            expect = self.read(self.dest + 40 + 4 * index)
            assert self.read(addr | 0x20000000) == ((expect if self.exact else 0) or HOOKS[addr])
        assert self.sdk_state() == self.expected_sdk


class EntryReplay(ReleaseSH):
    def run_module(self, module, stage_image):
        metadata = verify(module)
        flattened, memory = flatten_elf(self.elf[0])
        assert module[64:] == flattened and metadata["memory_bytes"] == memory
        start = self.symbols["__retail_stage_blob_start"][0]
        end = self.symbols["__retail_stage_blob_end"][0]
        assert end - start == len(stage_image) and not len(stage_image) & 3
        assert self.block(start, end - start) == stage_image
        stop = 0x8CE00000
        assert not any(self.block(stop, len(stage_image)))
        self.pc = ADDRESS
        for _ in range(10 + len(stage_image) * 2):
            if self.pc == stop:
                assert self.r[4] == self.symbols["__retail_map"][0]
                assert self.r[15] == 0x8CFF0000 and self.sr & ~1 == 0x500000F0
                assert self.block(stop, len(stage_image)) == stage_image
                assert len(self.writes) * 4 == len(stage_image)
                assert all(address == stop + index * 4 and count == 4
                           for index, (_, address, _, count) in enumerate(self.writes))
                return metadata["build"]
            pc = self.pc
            assert ADDRESS <= pc < ADDRESS + 0x20
            op = self.read(pc, 2)
            if op >> 8 == 0x8B:  # bf
                self.pc = pc + 2 if self.sr & 1 else pc + 4 + 2 * base.signed(op & 255, 8)
            elif op & 0xF0FF == 0x402B:  # jmp @Rn
                slot = self.read(pc + 2, 2)
                validate_delay_slot(slot, pc)
                target = self.r[(op >> 8) & 15]
                self.plain(slot, pc + 2)
                self.pc = target
            else:
                self.plain(op, pc)
                self.pc = pc + 2
        raise AssertionError("Stage entry copy exceeded its bounded instruction count")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("original", type=Path)
    parser.add_argument("build", type=Path, nargs="+")
    args = parser.parse_args()
    original = args.original.read_bytes()
    assert sha256(original).hexdigest() == "ee68a326da964fec5892b7226311f2293fd3a0bbf5bac496ec82991964d49bbd"
    for build in args.build:
        modules = list(build.glob("retail-toy-test-*.kui"))
        assert len(modules) == 1
        identifier = EntryReplay(build / "entry.elf").run_module(
            modules[0].read_bytes(), (build / "stage.bin").read_bytes())
        stage, resident, aux = (read_elf(build / f"{name}.elf")
                                for name in ("stage", "resident-sci", "native-ra-aux"))
        mode=int(modules[0].stem.rsplit('-',1)[1])
        for cache in (False, True):
            for payload in (0x0c400000, 0x8c500000, 0xacd60000):
                for exact in (False, True):
                    ToyBinder(stage, resident, aux, original, mode, payload, cache, exact).check()
        print(f"{build} ({identifier}): packaged entry/copy plus 12 linked Toy cold bind/init cases")


if __name__ == "__main__":
    main()
