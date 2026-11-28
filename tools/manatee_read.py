#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read every MANATEE-lineage sound driver of the CD-audio corpus for what the
CD-music output takes from its family (src/loader/toy_cdda_output.c,
docs/cdda-background-test-88.md).

usage: manatee_read.py SOUND_FILES_JSON DRIVER_DIR

SOUND_FILES_JSON is docs/evidence/cd-audio-corpus-2026-11-20/sound-files.json.
DRIVER_DIR holds the owner's files named by their SHA-256, as the CD-audio
sound-driver archive gives them. They are read and never copied: what is
printed is addresses, counts and hashes, no driver bytes. Run with python3 -I.
Needs the Python packages unicorn and capstone (pip install unicorn capstone).

Each image is run, not only read: loaded at 0 as the sd library loads it,
started at its reset vector on an ARM7 model with the sound chip's registers at
0x00800000, and then given 3,528 timer interrupts (eight times 441) the way the
chip raises them (FIQ with level 2 in register 0x2D00). From that run, one line
an image:
- the family the output's rule gives its version bytes and running mark;
- the word its timer interrupt counts, and by how much: 880 where the
  interrupt leaves out every 441st and counts every fourth (882 frames in five
  counts), 882 where it counts every fourth of all (176 frames a count);
- the timer's reload (0xD4: 44 frames) and the running mark it leaves at 0xF8;
- the family's channel records as its start-up leaves them: MANATEE 2's split
  bytes and stream-port base, MANATEE 1's pool of 16 with its register bases,
  NAMIHEI's stream-port records with theirs;
- the word it gives the game as the start of free memory (0x18000);
- the spare memory: the highest address the image can name below 0x18000
  (every literal word and every immediate of a data-processing instruction),
  the mask under which the command history at 0x14800 and MANATEE 2's MIDI
  ring at 0x15000 are indexed, and that after start-up the run touched nothing
  from the spare memory's start to 0x17FFF.
The expectations are the output's constants. A line that does not meet them
says why, and the exit status is 1.

What this is not: a console result, or the driver under a game's commands. The
start-up, the idle loop and the timer interrupt run; no host command is sent.
"""
import hashlib
import json
import os
import struct
import sys

try:
    import capstone
    from unicorn import Uc, UcError, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_HOOK_MEM_UNMAPPED
    from unicorn.arm_const import UC_ARM_REG_CPSR, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_SPSR
except ImportError as error:  # pragma: no cover
    sys.exit("manatee_read.py needs the unicorn and capstone packages: %s" % error)

GEN2, GEN1, GEN1_OLD, NAMIHEI = 1, 2, 3, 4
NAMES = {0: "none", GEN2: "MANATEE 2", GEN1: "MANATEE 1 from 1.01.37", GEN1_OLD: "MANATEE 1 before 1.01.37", NAMIHEI: "NAMIHEI"}
EXEC, SEGA = 0x43455845, 0x41474553
MEMORY = 0x200000
LOWEST = 0x18000
INTERRUPTS = 8 * 441
# The 1999.03.18 build damaged by a newline conversion: no driver runs from it.
DAMAGED = ("8a1433e941ec",)


def le32(data, at):
    return struct.unpack_from("<I", data, at)[0]


def date_hash(image):
    h = 0x811c9dc5
    for byte in image[0x50:0x5c]:
        h = ((h ^ byte) * 0x01000193) & 0xffffffff
    return (h ^ (h >> 16)) & 0xffff


def family_of(version, state):
    """The output's rule (driver() in src/loader/toy_cdda_output.c), written again here."""
    major, minor, build = version & 255, version >> 8 & 255, version >> 16 & 255
    if version >> 24:
        return 0
    if major == 2:
        return GEN2 if state == EXEC else 0
    if major != 1 or minor > 1:
        return 0
    if minor == 0:
        return NAMIHEI if state == SEGA else 0
    if build >= 0x37:
        return GEN1 if state == SEGA else 0
    if 0x28 < build < 0x32:
        return 0
    return GEN1_OLD if state == SEGA or (build < 0x28 and state == 0) else 0


def slow_count(version):
    """The output's rule for a count of 176 frames: MANATEE 1.01 before build 0x32."""
    return (version & 0xffff) == 0x0101 and (version >> 16 & 255) < 0x32


class Run:
    def __init__(self, image):
        self.image = image
        self.mu = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.mu.mem_map(0, MEMORY)
        self.mu.mem_map(0x800000, 0x10000)
        self.mu.mem_write(0, bytes(image))
        # What lay in sound memory is not zeros: a clear shows.
        self.mu.mem_write(len(image), b"\xa5" * (0x20000 - len(image)))
        self.level = 0
        self.registers = {}
        self.chip_writes = []
        self.phase = "start"
        self.touched = {"start": set(), "timer": set()}
        self.fiq_writes = set()
        self.fault = None
        self.mu.hook_add(UC_HOOK_MEM_WRITE, self.on_write)
        self.mu.hook_add(UC_HOOK_MEM_READ, self.on_read)
        self.mu.hook_add(UC_HOOK_MEM_UNMAPPED, self.on_fault)

    def on_fault(self, mu, access, address, size, value, data):
        self.fault = (address, mu.reg_read(UC_ARM_REG_PC))
        return False

    def on_write(self, mu, access, address, size, value, data):
        if address >= 0x800000:
            self.registers[address - 0x800000] = value & 0xffffffff
            self.chip_writes.append((self.phase, address - 0x800000, value & 0xffffffff))
            return
        if self.phase == "timer" and (mu.reg_read(UC_ARM_REG_CPSR) & 0x1f) == 0x11:
            self.fiq_writes.update(range(address & ~3, address + size, 4))
        if address >= len(self.image):
            self.touched[self.phase].update(range(address, address + size))

    def on_read(self, mu, access, address, size, value, data):
        if address >= 0x800000:
            register = address - 0x800000
            # 0x2D00: the interrupt's level; 0x2808: the MIDI ports, both empty.
            answer = self.level if register == 0x2d00 else 0x900 if register == 0x2808 else self.registers.get(register, 0)
            mu.mem_write(address, struct.pack("<I", answer)[:size])
        elif address >= len(self.image):
            self.touched[self.phase].update(range(address, address + size))

    def go(self, pc, count):
        try:
            self.mu.emu_start(pc, 0xffffffff, count=count)
        except UcError:
            return False
        return True

    def interrupt(self, level):
        mu = self.mu
        cpsr = mu.reg_read(UC_ARM_REG_CPSR)
        if cpsr & 0x40:
            return False
        pc = mu.reg_read(UC_ARM_REG_PC)
        self.level = level
        mu.reg_write(UC_ARM_REG_CPSR, (cpsr & ~0x3f) | 0x11 | 0xc0)
        mu.reg_write(UC_ARM_REG_SPSR, cpsr)
        mu.reg_write(UC_ARM_REG_LR, pc + 4)
        mu.reg_write(UC_ARM_REG_PC, 0x1c)
        return True

    def word(self, at):
        return le32(bytes(self.mu.mem_read(at, 4)), 0)


def named(image):
    """Every address from 0x14800 to below 0x18000 the image can name, and the
    masks beside the two it indexes from."""
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
    md.detail = True
    values, masks = set(), {}
    last = []
    for at in range(0x100, len(image) - 3, 4):
        word = le32(image, at)
        if 0x14800 <= word < LOWEST:
            values.add(word)
        decoded = list(md.disasm(image[at:at + 4], at))
        if not decoded:
            last = []
            continue
        instruction = decoded[0]
        immediates = [op.imm & 0xffffffff for op in instruction.operands if op.type == capstone.arm.ARM_OP_IMM]
        literal = None
        if instruction.mnemonic.startswith("ldr") and "[pc, #" in instruction.op_str:
            where = at + 8 + int(instruction.op_str.split("#")[1].rstrip("]"), 0)
            if 0 <= where < len(image) - 3:
                literal = le32(image, where)
        if instruction.mnemonic[:3] in ("mov", "add", "sub", "orr", "rsb", "cmp"):
            for value in immediates:
                if 0x14800 <= value < LOWEST:
                    values.add(value)
        last.append((instruction.mnemonic, immediates, literal))
        last = last[-24:]
        # The index of a ring: "and rX, rX, rMask" with the mask a literal just
        # before, within two dozen instructions of the ring's address.
        if instruction.mnemonic == "and" and not immediates:
            base = [v for m, imm, lit in last for v in imm + ([lit] if lit is not None else []) if v in (0x14800, 0x15000)]
            mask = [lit for m, imm, lit in last[-4:] if lit in (0x7ff, 0xfff)]
            if base and mask:
                masks.setdefault(base[-1], set()).add(mask[-1])
    return values, masks


def read(image):
    version = le32(image, 0x20)
    run = Run(image)
    facts = {"version": version, "hash": date_hash(image), "why": []}
    why = facts["why"]
    if not run.go(0, 3000000) or run.fault:
        why.append("the start-up did not run (fault %s)" % (run.fault,))
        return facts
    state = run.word(0xf8)
    family = family_of(version, state)
    facts.update(state=state, family=family)
    if not family:
        why.append("no family by the output's rule")
        return facts
    second = family == GEN2
    count_at = 0x12418 if second else 0x13418
    spare = 0x16000 if second else 0x15000
    facts.update(count_at=count_at, spare=spare)
    before = bytes(run.mu.mem_read(0, 0x20000))
    cleared = [a for a in range(len(image), LOWEST) if before[a] != 0xa5]
    facts["cleared_to"] = max(cleared) if cleared else 0
    if le32(before, 0x12408 if second else 0x13408) != LOWEST:
        why.append("free memory does not begin at 0x18000 by its own word")
    # The channel records as start-up leaves them.
    if second:
        split = before[0x12060:0x12063]
        facts["records"] = "split %d/%d/%d base %08x" % (split[0], split[1], split[2], le32(before, 0x12068))
        streams = split[2]
        if sum(split) != 64 or le32(before, 0x12068) != 0x00800000 + 0x80 * (64 - streams):
            why.append("the split bytes and the stream ports' base do not agree")
        if any(le32(before, 0x13800 + 0x40 * port) & 1 for port in range(streams)):
            why.append("a stream port playing after start-up")
        pair = (62, 63)
    elif family == NAMIHEI:
        ok = all(le32(before, 0x11200 + 0x40 * port + 8) == 0x00801c00 + 0x80 * port and
                 not le32(before, 0x11200 + 0x40 * port) & 1 for port in range(8))
        facts["records"] = "8 stream ports at 0x11200, bases %s" % ("proved" if ok else "NOT proved")
        if not ok:
            why.append("NAMIHEI's stream-port records do not carry their register bases")
        pair = (62, 63)
    else:
        ok = all(le32(before, 0x12080 + 12 * entry + 8) == 0x00801800 + 0x80 * entry and
                 not le32(before, 0x12080 + 12 * entry + 4) for entry in range(16))
        facts["records"] = "pool of 16 at 0x12080, bases %s" % ("proved" if ok else "NOT proved")
        if not ok:
            why.append("the pool's entries do not carry their register bases")
        pair = (61, 62) if family == GEN1 else (62, 63)
    facts["pair"] = pair
    # The timer interrupt.
    run.phase = "timer"
    taken = 0
    for _ in range(INTERRUPTS):
        if not run.interrupt(2):
            break
        taken += 1
        if not run.go(run.mu.reg_read(UC_ARM_REG_PC), 400) or run.fault:
            break
    after = bytes(run.mu.mem_read(0, 0x20000))
    grew = (le32(after, count_at) - le32(before, count_at)) & 0xffffffff
    facts.update(taken=taken, grew=grew)
    if taken != INTERRUPTS or run.fault:
        why.append("only %d of %d timer interrupts were taken (fault %s)" % (taken, INTERRUPTS, run.fault))
    expected = INTERRUPTS // 4 if slow_count(version) else INTERRUPTS * 440 // 441 // 4
    if grew != expected:
        why.append("the count grew by %d, the output's rule expects %d" % (grew, expected))
    if count_at not in run.fiq_writes:
        why.append("the timer interrupt does not write the count's word")
    others = sorted(a for a in run.fiq_writes if a >= 0x10000 and a != count_at)
    if others:
        why.append("the timer interrupt also writes %s" % " ".join("%x" % a for a in others))
    reloads = sorted(set(v for phase, register, v in run.chip_writes if phase == "timer" and register == 0x2890))
    facts["reload"] = reloads
    if reloads != [0xd4]:
        why.append("the timer is not reloaded for 44 frames")
    # The spare memory.
    values, masks = named(image)
    facts["highest"] = max(values) if values else 0
    facts["masks"] = masks
    wanted = {0x14800: {0x7ff}, 0x15000: {0xfff}} if second else {0x14800: {0x7ff}}
    if values != set(wanted):
        why.append("names %s from 0x14800 to below 0x18000" % " ".join("%x" % v for v in sorted(values)))
    if masks != wanted:
        why.append("the rings' index masks read %s" % masks)
    top = facts["highest"]
    if top not in wanted or top + max(wanted[top]) >= spare:
        why.append("the highest ring reaches the spare memory")
    used = sorted(a for a in run.touched["timer"] if spare <= a < LOWEST)
    if used:
        why.append("the run touched the spare memory at %x" % used[0])
    return facts


def main():
    inventory, directory = sys.argv[1:3]
    entries = [e for e in json.load(open(inventory)) if e.get("family") == "MANATEE"]
    lines, problems, missing = [], [], []
    rows = []
    for entry in entries:
        path = os.path.join(directory, entry["sha256"])
        if not os.path.isfile(path):
            missing.append(entry["sha256"])
            continue
        data = open(path, "rb").read()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            problems.append("%s: not the file the inventory names" % entry["sha256"])
            continue
        rows.append((entry, data[entry.get("image_offset", 0):]))
    rows.sort(key=lambda row: (bytes(row[1][0x20:0x22]), row[1][0x22], bytes(row[1][0x50:0x5c])))
    counted = {}
    for entry, image in rows:
        if entry["sha256"][:12] in DAMAGED:
            lines.append("%s %d.%02x.%02x %s damaged copy: not run, no expectation" % (
                entry["sha256"], image[0x20], image[0x21], image[0x22], image[0x50:0x5a].decode("ascii", "replace")))
            continue
        facts = read(image)
        version = facts["version"]
        text = "%s %d.%02x.%02x %s hash %04x" % (entry["sha256"], version & 255, version >> 8 & 255, version >> 16 & 255,
                                                  image[0x50:0x5a].decode("ascii", "replace"), facts["hash"])
        if "grew" in facts:
            counted[facts["family"]] = counted.get(facts["family"], 0) + 1
            text += " | %s, mark %08x | count %05x +%d in %d interrupts (%s frames a count), reload %s | channels %d, %d: %s | " \
                    "spare %05x..17fff: highest named %05x, masks %s, start-up clears to %05x" % (
                        NAMES[facts["family"]], facts["state"], facts["count_at"], facts["grew"], facts["taken"],
                        "176" if facts["grew"] == INTERRUPTS // 4 else "176.4", "/".join("%x" % v for v in facts["reload"]),
                        facts["pair"][0], facts["pair"][1], facts["records"], facts["spare"], facts["highest"],
                        " ".join("%x:%s" % (base, "/".join("%x" % m for m in sorted(mask))) for base, mask in sorted(facts["masks"].items())),
                        facts["cleared_to"])
        why = facts["why"]
        lines.append(text + (" | ok" if not why else " | FAIL: " + "; ".join(why)))
        if why:
            problems.append("%s %s" % (entry["sha256"][:12], "; ".join(why)))
    print("\n".join(lines))
    print("%d MANATEE-lineage images listed, %d absent, %d read: %s" % (
        len(entries), len(missing), sum(counted.values()),
        ", ".join("%d %s" % (n, NAMES[f]) for f, n in sorted(counted.items()))))
    if missing:
        print("absent: " + " ".join(sha[:12] for sha in missing))
    if problems:
        print("FAIL\n" + "\n".join(problems))
        return 1
    print("PASS every image read meets what the CD-music output takes from its family")
    return 0


if __name__ == "__main__":
    sys.exit(main())
