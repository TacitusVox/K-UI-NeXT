#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the shipped dual-reader graph, SH literal operands and its owned RAM ranges."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_vmu_native_worker_gateway import read_elf, symbol_bytes
from runtime_package import flatten_elf, verify
from check_vmu_native_readahead import private_stack


def require(ok, message):
    if not ok:
        raise ValueError(message)


def payload(elf):
    loads = [p for p in elf[2] if p[0] == 1 and p[4]]
    start = min(p[3] for p in loads)
    end = max(p[3] + p[4] for p in loads)
    require(0 < end-start < 0x400000, "invalid initialized SH span")
    data = bytearray(end-start)
    for _, offset, _, target, size, _, _, _ in loads:
        data[target-start:target-start+size] = elf[0][offset:offset+size]
    return start, bytes(data)


def segment(elf, addr, size):
    for p in elf[2]:
        if p[0] == 1 and p[2] <= addr <= addr+size <= p[2]+p[4]:
            offset = p[1]+addr-p[2]
            return elf[0][offset:offset+size]
    raise ValueError(f"address 0x{addr:08x} outside linked file data")


def embedded(elf, begin, end, data):
    first, last = elf[4][begin][0], elf[4][end][0]
    require(last-first == len(data) and segment(elf, first, last-first) == data,
            f"mismatched embedded component {begin}")


def literals(elf, name):
    pc, code, section = symbol_bytes(elf, name, 2)
    values = set()
    for i in range(0, len(code)-1, 2):
        op, = struct.unpack_from("<H", code, i)
        size = {9: 2, 13: 4}.get(op >> 12)
        if size is None:
            continue
        slot = ((pc+i+4) & -size) + size*(op & 255)
        if section[3] <= slot <= section[3]+section[5]-size:
            values.add(int.from_bytes(segment(elf, slot, size), "little"))
    return values


def required_loads(elf, name, expected):
    missing = set(expected) - literals(elf, name)
    require(not missing, f"{name} lacks linked SH operands {sorted(map(hex, missing))}")


def check_transfer(stage, map_bytes, region, negative_controls):
    """Verify the linked instructions supplying both memcpy calls and the bind gate."""
    first, code, _ = symbol_bytes(stage, "_native_passive_run", 2)
    symbols = stage[4]
    def check(blob):
        ops = struct.unpack("<"+"H"*(len(blob)//2), blob)
        def operand(i):
            size = {9:2, 13:4}.get(ops[i] >> 12)
            if size is None:
                return None
            pointer = ((first+2*i+4)&-size)+size*(ops[i]&255)
            return int.from_bytes(segment(stage, pointer, size), "little")
        def load(value, begin=0, length=None, register=None):
            candidates = [i for i in range(begin, min(len(ops), begin+(length or len(ops))))
                          if (register is None or ops[i]>>8&15 == register) and operand(i)==value]
            require(candidates, f"missing SH argument {value:x}")
            return candidates[0]
        def call_after(i, limit=12):
            matches = [j for j in range(i+1, i+limit) if ops[j]&0xf0ff==0x400b]
            require(matches, "missing indirect SH call in installer handoff")
            return matches[0]
        map_dest = load(0x20012000)
        map_call = call_after(map_dest)
        count = load(map_bytes,map_dest,map_call-map_dest,6)
        load(symbols["_manifest"][0],map_dest,map_call-map_dest,5)
        region_dest = load(0x20000000+region,map_call,120,4)
        region_call = call_after(region_dest)
        load(0x760,region_dest,region_call-region_dest,6)
        load(symbols["_native_background_boot_region"][0],region_dest,region_call-region_dest,5)
        require(ops[map_call] == ops[region_call], "snapshot copies call different functions")
        load(symbols["_memcpy"][0],0,map_call,(ops[map_call]>>8)&15)
        publish = load(symbols["_kui_native_passive_publish"][0],region_call,8)
        publish_call = call_after(publish)
        require((ops[publish_call]>>8&15)==(ops[publish]>>8&15), "vector publish callback mismatch")
        bind = load(symbols["_native_low_bind_map"][0],publish_call,8)
        bind_call = call_after(bind)
        require(ops[bind_call]>>8&15==ops[bind]>>8&15 and ops[bind_call+2]==0x8801 and
                ops[bind_call+3]>>8 in (0x8b,0x8f), "reader must accept exactly one before worker install")
        return (count,region_call,publish_call,bind_call+2)
    sites = check(code)
    if negative_controls:
        for i in sites:
            altered = bytearray(code)
            struct.pack_into("<H",altered,2*i,9)  # replace argument/call/gate with a NOP
            try:
                check(altered)
            except ValueError:
                continue
            raise ValueError("linked handoff accepted mutated size, call or acceptance check")
    return len(sites) if negative_controls else 0


def audit(directory, kui, slots, negative_controls=False):
    files = {name: read_elf(directory / (name+".elf")) for name in
             ("entry", "stage", "native-worker", "native-ra-aux", "resident-sci", "resident-legacy")}
    entry, stage, worker, aux = (files[name] for name in ("entry", "stage", "native-worker", "native-ra-aux"))
    package = kui.read_bytes()
    identity = verify(package)
    body, memory = flatten_elf(entry[0])
    require(package[64:] == body and memory == identity["memory_bytes"], "KUI differs from linked entry")
    embedded(entry, "__retail_stage_blob_start", "__retail_stage_blob_end", payload(stage)[1])
    for bus in ("scif", "sci", "ide", "scia", "legacy"):
        low = files["resident-legacy" if bus == "legacy" else "resident-sci"]
        embedded(stage, "__retail_resident_"+bus+"_blob_start", "__retail_resident_"+bus+"_blob_end",
                 payload(low)[1])
    embedded(stage, "__native_worker_blob_begin", "__native_worker_blob_end", payload(worker)[1])
    embedded(stage, "_native_ra_aux_blob_start", "_native_ra_aux_blob_end", payload(aux)[1])
    for name, first, end in (("native-worker", "__native_worker_reloc_begin", "__native_worker_reloc_end"),
                             ("native-ra-aux", "_native_ra_aux_reloc_start", "_native_ra_aux_reloc_end")):
        embedded(stage, first, end, (directory/(name+".reloc")).read_bytes())

    map_at, region, region_size, aux_at, lease = 0x12000, {64:0x12400, 160:0x12880}[slots], 0x760, 0x13000, 0x47000
    map_bytes = 252+12*slots
    require(not region % 32 and map_at+map_bytes <= region and region+region_size <= aux_at,
            "retained map, vector state and auxiliary overlap")
    # SH exceptions add 0x100/400/600 to VBR. Relative region vector members
    # are 0/300/500. This must hold for every admitted lease alignment.
    for low in range(0, 0x1000, 32):
        start = 0x8c300000+low
        for displacement, member in ((0x100,0),(0x400,0x300),(0x600,0x500)):
            vector = start+region-0x100+displacement
            require(vector == start+region+member and not vector%32, "misaligned relocated interrupt vector")
    symbols = stage[4]
    m, msize = symbols["_manifest"][:2]
    r, rsize = symbols["_native_background_boot_region"][:2]
    require(msize >= map_bytes and rsize == region_size and not r%32 and
            min(m+msize, r+rsize) <= max(m,r), "invalid or overlapping temporary stage workspaces")
    required_loads(stage, "_native_passive_run",
                   [map_at, map_at+0x20000000, region, region+0x20000000, map_bytes, region_size, lease])
    rejected = check_transfer(stage, map_bytes, region, negative_controls)
    for name in ("_native_sdk_sci_route_bind", "_native_readahead_bind"):
        required_loads(stage, name, [region, lease])
    required_loads(aux, "_kui_native_readahead_aux_init", [region, lease])
    required_loads(worker, "_reader_capture_region", [region])
    _, table, _ = symbol_bytes(stage, "_native_low_bindings_table", 1)
    hooks = ("_kui_vmu_observe_heap_hook", "_kui_vmu_observe_return_hook", "_kui_vmu_observe_boot_control",
             "_kui_vmu_native_heap_original", "_kui_vmu_observe_sd_lease", "_kui_vmu_native_background_manifest_bind")
    for i, kind in enumerate(("resident-sci", "resident-legacy")):
        low = files[kind]
        require(struct.unpack_from("<6I", table, i*24) == tuple(low[4][h][0] for h in hooks),
                f"stage launch/return callback table mismatch for {kind}")
        required_loads(low, "_kui_vmu_native_background_init", [map_bytes, region_size])
        required_loads(low, "_kui_vmu_native_background_manifest_bind", [map_at, region, map_bytes, region_size])
    # The legacy non-SDK route restores SPC/SP on its nested return and
    # loads VBR = *region_ptr - 0x100, using the current rebound pointer.
    legacy = files["resident-legacy"]
    rehook = legacy[4]["_kui_retail_rehook"][0]
    words = struct.unpack("<110H", segment(legacy, rehook, 220))
    def loads_region_pointer(i):
        op = words[i-1]
        pc = rehook+2*(i-1)
        return op >> 8 == 0xd1 and int.from_bytes(segment(legacy, ((pc+4)&~3)+4*(op&255), 4), "little") == \
            legacy[4]["_kui_retail_async_region_storage"][0]
    require(any(words[i:i+4] == (0x6112,0x7180,0x7180,0x412e) and loads_region_pointer(i)
                for i in range(1,len(words)-3)),
            "legacy return no longer loads VBR from region minus 0x100")
    require(0x434e in words and 0x002b in words, "legacy nested return lacks SPC restore or RTE")
    worker_top = worker[4]["__vmu_native_worker_stack_top"][0] & 0x1fffffff
    worker_bottom = worker[4]["__vmu_native_worker_stack_bottom"][0] & 0x1fffffff
    require(worker_top - 0x0c100000 == map_at and worker_top-worker_bottom == 0xc00,
            "retained map interferes with worker stack")
    # In the survey, the auxiliary is larger and the data/ring begins later.
    aux_size = len(payload(aux)[1])
    bottom, top = (aux[4][n][0] & 0x1fffffff for n in ("__native_ra_stack_bottom", "__native_ra_stack_top"))
    aux_span = bottom-0x0c421000
    # Test 84 (ROOM) gives 2,304 bytes of the 4 KiB stack to code, at the same top.
    require(aux_size <= aux_span and ((aux_span in (0x3000,0x6000) and top-bottom == 0x1000) or
                                      (aux_span == 0x6900 and top-bottom == 0x700)),
            "auxiliary executable and stack overlap")
    if "_kui_native_ra_extent_end" in aux[4]:
        stack = private_stack(directory, files["resident-sci"], self_test=negative_controls)
        stack_result = stack["total_bytes"]
    else:
        # Test 83 links its wire reader instead of the normal lookahead seeder.
        require(aux_span in (0x6000,0x6900), "normal auxiliary lost physical-extent traversal")
        stack_result = "extent traversal removed by linker"
    return dict(package_sha256=hashlib.sha256(package).hexdigest(), package_build=identity["build"],
                slots=slots, retained_map_bytes=map_bytes, region_offset=hex(region), region_bytes=region_size,
                rejected_handoff_mutations=rejected, extent_stack_with_margins=stack_result,
                bytes_before_aux=aux_at-region-region_size, read_cache_bytes=lease-aux_at-aux_span-(top-bottom))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("kui", type=Path)
    parser.add_argument("--slots", type=int, choices=(64,160), default=160)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    print(json.dumps(audit(args.build, args.kui, args.slots, args.self_test), indent=2))
