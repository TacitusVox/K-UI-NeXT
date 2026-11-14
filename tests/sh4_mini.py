# SPDX-License-Identifier: GPL-3.0-only
"""A small SH-4 interpreter for running linked functions on test data.

No emulator is available where these checks run, so the compiler's output
for the reader's inner loops was only ever read. This executes it: the
integer subset of SH-4 that GCC emits for such code, with delayed branches,
on a flat little-endian memory in which the P0, P1 and P2 views of main
memory are one (no cache is modelled; cache instructions are counted and
otherwise do nothing). An instruction outside the subset, or an access to
an address nothing was mapped at, stops the run with an error: a function
that needs more than this is not one this tool can vouch for.

It checks the code as linked. It does not check the Dreamcast.
"""
import struct
import subprocess

M32 = 0xFFFFFFFF


class Fault(Exception):
    pass


def _s8(v):
    return v - 0x100 if v & 0x80 else v


def _s12(v):
    return v - 0x1000 if v & 0x800 else v


def _s16(v):
    return v - 0x10000 if v & 0x8000 else v


class Machine:
    RETURN = 0xDEAD0000  # PR of the outermost call

    def __init__(self):
        self.regions = []  # (physical base, bytearray)
        self.devices = {}  # address -> (read(width) or None, write(value, width) or None)
        self.r = [0] * 16
        self.pc = 0
        self.pr = 0
        self.t = 0
        self.macl = 0
        self.mach = 0
        self.executed = 0
        self.counts = {}  # mnemonic -> times executed
        self.loads = self.stores = 0
        self.touched = set()  # physical addresses of every data access
        self.cache_ops = []  # (mnemonic, address)

    # ---- memory ----
    @staticmethod
    def physical(address):
        return address & 0x1FFFFFFF

    def map(self, address, data):
        """Map bytes at any view of an address; returns the bytearray."""
        store = bytearray(data)
        self.regions.append((self.physical(address), store))
        return store

    def _find(self, address, width):
        if address >= 0xE0000000:
            raise Fault(f"access to {address:#x}: not memory")
        p = self.physical(address)
        for base, store in self.regions:
            if base <= p and p + width <= base + len(store):
                return store, p - base
        raise Fault(f"access to unmapped {address:#x} (pc {self.pc:#x})")

    def read(self, address, width):
        address &= M32
        if address in self.devices and self.devices[address][0]:
            return self.devices[address][0](width) & ((1 << 8 * width) - 1)
        if address & (width - 1):
            raise Fault(f"unaligned {width}-byte read at {address:#x} (pc {self.pc:#x})")
        store, at = self._find(address, width)
        self.loads += 1
        self.touched.add(self.physical(address))
        return int.from_bytes(store[at:at + width], "little")

    def write(self, address, value, width):
        address &= M32
        if address in self.devices and self.devices[address][1]:
            self.devices[address][1](value & ((1 << 8 * width) - 1), width)
            return
        if address & (width - 1):
            raise Fault(f"unaligned {width}-byte write at {address:#x} (pc {self.pc:#x})")
        store, at = self._find(address, width)
        self.stores += 1
        self.touched.add(self.physical(address))
        store[at:at + width] = (value & ((1 << 8 * width) - 1)).to_bytes(width, "little")

    def bytes_at(self, address, count):
        store, at = self._find(address, count)
        return bytes(store[at:at + count])

    # ---- loading ----
    def load_elf(self, path):
        """Map every loadable segment of a 32-bit little-endian ELF."""
        with open(path, "rb") as f:
            image = f.read()
        if image[:6] != b"\x7fELF\x01\x01":
            raise Fault("not a 32-bit little-endian ELF")
        phoff, = struct.unpack_from("<I", image, 28)
        phentsize, phnum = struct.unpack_from("<HH", image, 42)
        for i in range(phnum):
            kind, offset, vaddr, _, filesz, memsz = struct.unpack_from("<IIIIII", image, phoff + i * phentsize)
            if kind != 1 or not memsz:
                continue
            self.map(vaddr, image[offset:offset + filesz] + bytes(memsz - filesz))

    @staticmethod
    def symbols(path, nm="sh-elf-nm"):
        out = subprocess.run([nm, path], check=True, capture_output=True, text=True).stdout
        table = {}
        for line in out.splitlines():
            parts = line.split()
            if len(parts) == 3:
                table[parts[2]] = int(parts[0], 16)
        return table

    # ---- running ----
    def call(self, address, *args, stack=None, limit=50_000_000):
        """Run the function at `address` with up to four integer arguments
        (SH C ABI: R4..R7) and return R0."""
        if len(args) > 4:
            raise Fault("more than four arguments are passed on the stack: not supported here")
        for i, a in enumerate(args):
            self.r[4 + i] = a & M32
        if stack is not None:
            self.r[15] = stack
        saved = [self.r[i] for i in range(8, 16)]
        self.pr = self.RETURN
        self.pc = address
        steps = 0
        while self.pc != self.RETURN:
            self.step()
            steps += 1
            if steps > limit:
                raise Fault("step limit reached")
        if [self.r[i] for i in range(8, 16)] != saved:
            raise Fault("a callee-saved register or the stack pointer was not restored")
        return self.r[0]

    def _count(self, name):
        self.counts[name] = self.counts.get(name, 0) + 1
        self.executed += 1

    def _branch(self, target, delayed):
        if delayed:
            slot = self.pc + 2
            self.pc = slot
            self._execute(self.read_code(slot), in_slot=True)
        self.pc = target & M32

    def read_code(self, address):
        store, at = self._find(address, 2)
        return int.from_bytes(store[at:at + 2], "little")

    def step(self):
        op = self.read_code(self.pc)
        before = self.pc
        self._execute(op, in_slot=False)
        if self.pc == before:
            self.pc = before + 2

    def _execute(self, op, in_slot):
        r = self.r
        n = op >> 8 & 15
        m = op >> 4 & 15
        top = op >> 12
        low = op & 15
        pc = self.pc
        c = self._count

        def forbid():
            if in_slot:
                raise Fault(f"branch in a delay slot at {pc:#x}")

        if op == 0x0009:
            c("nop")
        elif op == 0x000B:
            forbid(); c("rts"); self._branch(self.pr, True)
        elif op == 0x0008:
            c("clrt"); self.t = 0
        elif op == 0x0018:
            c("sett"); self.t = 1
        elif top == 0x6 and low == 0x3:
            c("mov"); r[n] = r[m]
        elif top == 0xE:
            c("mov#"); r[n] = _s8(op & 255) & M32
        elif top == 0x9:
            c("mov.w@pc"); r[n] = _s16(self.read(pc + 4 + (op & 255) * 2, 2)) & M32
        elif top == 0xD:
            c("mov.l@pc"); r[n] = self.read((pc & ~3) + 4 + (op & 255) * 4, 4)
        elif top == 0x2 and low <= 2:
            c(("mov.b", "mov.w", "mov.l")[low] + ",@"); self.write(r[n], r[m], 1 << low)
        elif top == 0x6 and low <= 2:
            width = 1 << low
            c(("mov.b", "mov.w", "mov.l")[low] + "@,")
            v = self.read(r[m], width)
            r[n] = (_s8(v) if low == 0 else _s16(v) if low == 1 else v) & M32
        elif top == 0x2 and 4 <= low <= 6:
            width = 1 << (low - 4)
            c(("mov.b", "mov.w", "mov.l")[low - 4] + ",@-")
            value = r[m]
            r[n] = (r[n] - width) & M32
            self.write(r[n], value, width)
        elif top == 0x6 and 4 <= low <= 6:
            width = 1 << (low - 4)
            c(("mov.b", "mov.w", "mov.l")[low - 4] + "@+,")
            v = self.read(r[m], width)
            if n != m:
                r[m] = (r[m] + width) & M32
            r[n] = (_s8(v) if low == 4 else _s16(v) if low == 5 else v) & M32
        elif op >> 8 == 0x80:
            c("mov.b r0,@(d)"); self.write(r[m] + (op & 15), r[0], 1)
        elif op >> 8 == 0x81:
            c("mov.w r0,@(d)"); self.write(r[m] + (op & 15) * 2, r[0], 2)
        elif top == 0x1:
            c("mov.l,@(d)"); self.write(r[n] + (op & 15) * 4, r[m], 4)
        elif op >> 8 == 0x84:
            c("mov.b @(d),r0"); r[0] = _s8(self.read(r[m] + (op & 15), 1)) & M32
        elif op >> 8 == 0x85:
            c("mov.w @(d),r0"); r[0] = _s16(self.read(r[m] + (op & 15) * 2, 2)) & M32
        elif top == 0x5:
            c("mov.l@(d),"); r[n] = self.read(r[m] + (op & 15) * 4, 4)
        elif top == 0x0 and 4 <= low <= 6:
            c(("mov.b", "mov.w", "mov.l")[low - 4] + ",@(r0)")
            self.write(r[n] + r[0], r[m], 1 << (low - 4))
        elif top == 0x0 and 0xC <= low <= 0xE:
            width = 1 << (low - 0xC)
            c(("mov.b", "mov.w", "mov.l")[low - 0xC] + "@(r0),")
            v = self.read(r[m] + r[0], width)
            r[n] = (_s8(v) if low == 0xC else _s16(v) if low == 0xD else v) & M32
        elif op >> 8 == 0xC7:
            c("mova"); r[0] = ((pc & ~3) + 4 + (op & 255) * 4) & M32
        elif top == 0x0 and op & 0xFF == 0x29:
            c("movt"); r[n] = self.t
        elif top == 0x6 and low == 0x8:
            c("swap.b"); r[n] = (r[m] & 0xFFFF0000) | (r[m] & 0xFF) << 8 | (r[m] >> 8 & 0xFF)
        elif top == 0x6 and low == 0x9:
            c("swap.w"); r[n] = (r[m] << 16 | r[m] >> 16) & M32
        elif top == 0x2 and low == 0xD:
            c("xtrct"); r[n] = (r[m] << 16 | r[n] >> 16) & M32
        elif top == 0x3 and low == 0xC:
            c("add"); r[n] = (r[n] + r[m]) & M32
        elif top == 0x7:
            c("add#"); r[n] = (r[n] + _s8(op & 255)) & M32
        elif top == 0x3 and low == 0xE:
            c("addc"); total = r[n] + r[m] + self.t; r[n] = total & M32; self.t = total >> 32
        elif top == 0x3 and low == 0x8:
            c("sub"); r[n] = (r[n] - r[m]) & M32
        elif top == 0x3 and low == 0xA:
            c("subc"); total = r[n] - r[m] - self.t; r[n] = total & M32; self.t = 1 if total < 0 else 0
        elif top == 0x6 and low == 0xB:
            c("neg"); r[n] = -r[m] & M32
        elif top == 0x6 and low == 0xA:
            c("negc"); total = -r[m] - self.t; r[n] = total & M32; self.t = 1 if total < 0 else 0
        elif op >> 8 == 0x88:
            c("cmp/eq#"); self.t = int(r[0] == (_s8(op & 255) & M32))
        elif top == 0x3 and low == 0x0:
            c("cmp/eq"); self.t = int(r[n] == r[m])
        elif top == 0x3 and low == 0x2:
            c("cmp/hs"); self.t = int(r[n] >= r[m])
        elif top == 0x3 and low == 0x6:
            c("cmp/hi"); self.t = int(r[n] > r[m])
        elif top == 0x3 and low == 0x3:
            c("cmp/ge"); self.t = int(_s32(r[n]) >= _s32(r[m]))
        elif top == 0x3 and low == 0x7:
            c("cmp/gt"); self.t = int(_s32(r[n]) > _s32(r[m]))
        elif top == 0x4 and op & 0xFF == 0x11:
            c("cmp/pz"); self.t = int(_s32(r[n]) >= 0)
        elif top == 0x4 and op & 0xFF == 0x15:
            c("cmp/pl"); self.t = int(_s32(r[n]) > 0)
        elif top == 0x2 and low == 0xC:
            c("cmp/str"); x = r[n] ^ r[m]; self.t = int(any(not (x >> s & 255) for s in (0, 8, 16, 24)))
        elif top == 0x4 and op & 0xFF == 0x10:
            c("dt"); r[n] = (r[n] - 1) & M32; self.t = int(r[n] == 0)
        elif top == 0x6 and low == 0xC:
            c("extu.b"); r[n] = r[m] & 0xFF
        elif top == 0x6 and low == 0xD:
            c("extu.w"); r[n] = r[m] & 0xFFFF
        elif top == 0x6 and low == 0xE:
            c("exts.b"); r[n] = _s8(r[m] & 0xFF) & M32
        elif top == 0x6 and low == 0xF:
            c("exts.w"); r[n] = _s16(r[m] & 0xFFFF) & M32
        elif top == 0x0 and low == 0x7:
            c("mul.l"); self.macl = r[n] * r[m] & M32
        elif top == 0x2 and low == 0xE:
            c("mulu.w"); self.macl = (r[n] & 0xFFFF) * (r[m] & 0xFFFF) & M32
        elif top == 0x2 and low == 0xF:
            c("muls.w"); self.macl = _s16(r[n] & 0xFFFF) * _s16(r[m] & 0xFFFF) & M32
        elif top == 0x0 and op & 0xFF == 0x1A:
            c("sts macl"); r[n] = self.macl
        elif top == 0x0 and op & 0xFF == 0x0A:
            c("sts mach"); r[n] = self.mach
        elif top == 0x2 and low == 0x9:
            c("and"); r[n] &= r[m]
        elif op >> 8 == 0xC9:
            c("and#"); r[0] &= op & 255
        elif top == 0x6 and low == 0x7:
            c("not"); r[n] = ~r[m] & M32
        elif top == 0x2 and low == 0xB:
            c("or"); r[n] |= r[m]
        elif op >> 8 == 0xCB:
            c("or#"); r[0] |= op & 255
        elif top == 0x2 and low == 0x8:
            c("tst"); self.t = int(not r[n] & r[m])
        elif op >> 8 == 0xC8:
            c("tst#"); self.t = int(not r[0] & (op & 255))
        elif top == 0x2 and low == 0xA:
            c("xor"); r[n] ^= r[m]
        elif op >> 8 == 0xCA:
            c("xor#"); r[0] ^= op & 255
        elif top == 0x4 and op & 0xFF in (0x00, 0x20):
            c("shll" if op & 0x20 == 0 else "shal"); self.t = r[n] >> 31; r[n] = r[n] << 1 & M32
        elif top == 0x4 and op & 0xFF == 0x01:
            c("shlr"); self.t = r[n] & 1; r[n] >>= 1
        elif top == 0x4 and op & 0xFF == 0x21:
            c("shar"); self.t = r[n] & 1; r[n] = (_s32(r[n]) >> 1) & M32
        elif top == 0x4 and op & 0xFF in (0x08, 0x18, 0x28):
            c("shll" + {0x08: "2", 0x18: "8", 0x28: "16"}[op & 0xFF])
            r[n] = r[n] << {0x08: 2, 0x18: 8, 0x28: 16}[op & 0xFF] & M32
        elif top == 0x4 and op & 0xFF in (0x09, 0x19, 0x29):
            c("shlr" + {0x09: "2", 0x19: "8", 0x29: "16"}[op & 0xFF])
            r[n] >>= {0x09: 2, 0x19: 8, 0x29: 16}[op & 0xFF]
        elif top == 0x4 and low == 0xD:
            c("shld"); s = _s32(r[m])
            r[n] = (r[n] << (s & 31)) & M32 if s >= 0 else (0 if not s & 31 else r[n] >> (-s & 31))
        elif top == 0x4 and low == 0xC:
            c("shad"); s = _s32(r[m])
            if s >= 0:
                r[n] = (r[n] << (s & 31)) & M32
            elif not s & 31:
                r[n] = M32 if r[n] >> 31 else 0
            else:
                r[n] = (_s32(r[n]) >> (-s & 31)) & M32
        elif top == 0x4 and op & 0xFF == 0x04:
            c("rotl"); self.t = r[n] >> 31; r[n] = (r[n] << 1 | self.t) & M32
        elif top == 0x4 and op & 0xFF == 0x05:
            c("rotr"); self.t = r[n] & 1; r[n] = r[n] >> 1 | self.t << 31
        elif top == 0x4 and op & 0xFF == 0x24:
            c("rotcl"); carry = r[n] >> 31; r[n] = (r[n] << 1 | self.t) & M32; self.t = carry
        elif top == 0x4 and op & 0xFF == 0x25:
            c("rotcr"); carry = r[n] & 1; r[n] = r[n] >> 1 | self.t << 31; self.t = carry
        elif op >> 8 in (0x89, 0x8B, 0x8D, 0x8F):
            forbid()
            kind = op >> 8
            taken = bool(self.t) == (kind in (0x89, 0x8D))
            c({0x89: "bt", 0x8B: "bf", 0x8D: "bt.s", 0x8F: "bf.s"}[kind])
            target = pc + 4 + _s8(op & 255) * 2
            delayed = kind in (0x8D, 0x8F)
            if taken:
                self._branch(target, delayed)
            elif delayed:
                self._branch(pc + 4, True)
        elif top == 0xA:
            forbid(); c("bra"); self._branch(pc + 4 + _s12(op & 0xFFF) * 2, True)
        elif top == 0xB:
            forbid(); c("bsr"); target = pc + 4 + _s12(op & 0xFFF) * 2; self.pr = pc + 4; self._branch(target, True)
        elif top == 0x4 and op & 0xFF == 0x2B:
            forbid(); c("jmp"); self._branch(r[n], True)
        elif top == 0x4 and op & 0xFF == 0x0B:
            forbid(); c("jsr"); target = r[n]; self.pr = pc + 4; self._branch(target, True)
        elif top == 0x0 and op & 0xFF == 0x23:
            forbid(); c("braf"); self._branch(pc + 4 + r[n], True)
        elif top == 0x4 and op & 0xFF == 0x22:
            c("sts.l pr"); r[n] = (r[n] - 4) & M32; self.write(r[n], self.pr, 4)
        elif top == 0x4 and op & 0xFF == 0x26:
            c("lds.l pr"); self.pr = self.read(r[n], 4); r[n] = (r[n] + 4) & M32
        elif top == 0x0 and op & 0xFF == 0x2A:
            c("sts pr"); r[n] = self.pr
        elif top == 0x4 and op & 0xFF == 0x2A:
            c("lds pr"); self.pr = r[n]
        elif top == 0x0 and op & 0xFF in (0x83, 0x93, 0xA3, 0xB3):
            name = {0x83: "pref", 0x93: "ocbi", 0xA3: "ocbp", 0xB3: "ocbwb"}[op & 0xFF]
            c(name); self.cache_ops.append((name, r[n]))
        else:
            raise Fault(f"instruction {op:04x} at {pc:#x} is outside the subset this interpreter runs")


def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v
