#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The launcher's streamed Maple witness is the discovery's own.

K-UI cannot hold a program larger than its free memory (under 8 MiB on the
console), so for such a program it searches the stream for the witness of the
SDK check's first step (src/apps/games_native_witness.inc) and refuses the
combined Background module where there is none. That is sound only while the
launcher's table and bounds are exactly the discovery's
(src/core/vmu_native_discovery.c) and that step is still what every stage's
check starts with. This test reads the sources and fails when either moves:
a new Maple family, another table or other bounds need the launcher's search
changed in the same commit.
"""
import hashlib
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAIR = re.compile(r"\{\s*(-?\d+)\s*,\s*0x([0-9a-fA-F]{4})\s*\}")

# The code the launcher's refusal rests on, pinned whole: (file, from this text,
# through this text, SHA-256). The statements checked one by one below say what
# matters in it; these pins catch what they cannot name, such as a second way
# to accept a driver, a check skipped in some mode, or a longer program.
#
# A pin that fails means that code changed. Before updating it, answer for the
# change: can a combined module's stage now start a program that has no even
# site, from 0x53c to its last 0x1b8 bytes, at which the 32 words of `words`
# match? If it can (a new Maple family, a mode that skips the check, a program
# longer than the boot file), change src/apps/games_native_witness.inc and its
# use in games_native_cache.inc in the same commit, so that the launcher no
# longer refuses such a program; then update the pin.
PINS = (
    ("the Maple discovery", "src/core/vmu_native_discovery.c", None, None,
     "0e59d65f37f8908787fedbb5397d83dccbc73bb1929a38581118d96b60684a72"),
    ("the program extent", "src/core/vmu_native_program.c", "static size_t arena_tail(", None,
     "a8d5ac9b47f962e86811640d5b6149e84fb56e29e271359cd20a7c918bac2900"),
    ("the layout step", "src/core/vmu_native_layout.c", "bool kui_vmu_native_layout_discover(",
     "static bool bind_failure(",
     "d7f70520f83caaa661a82c5615eff3fc580b15350025cae9f4f411503c37360c"),
    ("the stage's check", "src/loader/native_cold_discovery.inc", None, None,
     "699bea267e9570ef9b735c99dbbf5791bd4786da6e5783a1f9385d6dcaa859a5"),
    ("the stage's relay up to its check", "src/loader/vmu_native_heap_probe_stage.c",
     "void kui_retail_stage_relay(const uint32_t *frame,uint32_t ccr) {",
     "native_report(2u,native_heap.initializer_slot);",
     "486cc29adc22a45a152331ea23b175a40be8b0967f1f392f535dde4231138745"),
)


def pinned(path, begin, end):
    text = (ROOT / path).read_text()
    start = 0
    if begin is not None:
        assert text.count(begin) == 1, f"{path}: {text.count(begin)} places begin with {begin!r}"
        start = text.index(begin)
    stop = len(text)
    if end is not None:
        assert end in text[start:], f"{path}: no {end!r} after {begin!r}"
        stop = text.index(end, start) + len(end)
    return hashlib.sha256(text[start:stop].encode()).hexdigest()


def table(text, declaration):
    found = re.findall(re.escape(declaration) + r"\s*=\s*\{(.*?)\};", text, re.S)
    assert len(found) == 1, f"{declaration}: {len(found)} tables"
    return [(int(offset), int(value, 16)) for offset, value in PAIR.findall(found[0])]


def main():
    core = (ROOT / "src/core/vmu_native_discovery.c").read_text()
    gui = (ROOT / "src/apps/games_native_witness.inc").read_text()
    layout = (ROOT / "src/core/vmu_native_layout.c").read_text()
    resolution = (ROOT / "src/core/vmu_native_resolution_cache.c").read_text()
    stage = (ROOT / "src/loader/native_cold_discovery.inc").read_text()
    preflight = (ROOT / "src/apps/games_native_cache.inc").read_text()

    # The discovery's site table is `words`, and the launcher's is the same, in
    # order. Its other three tables belong to startup_p2_at, a later step that
    # accepts no site. A fifth table is a new family.
    assert core.count("struct witness {int16_t offset;uint16_t value;};") == 1
    assert re.findall(r"static const struct witness (\w+)\[\]", core) == ["words", "w", "ordinary", "extended"], \
        "another witness table: a new family?"
    later = core[core.index("static bool startup_p2_at("):]
    assert all(later.count(f"static const struct witness {name}[]") == 1 for name in ("w", "ordinary", "extended"))
    assert len(re.findall(r"\bwords\s*[\[)]", core)) == 5 and not re.search(r"\bwords\s*[\[)]", later), \
        "the site table is read outside sdk_at"
    words = table(core, "static const struct witness words[]")
    mine = table(gui, "static const struct native_witness_word native_witness_words[]")
    assert len(words) == 32 and words == mine, "the launcher's witness is not the discovery's"

    # sdk_at tests the whole table before anything else, and nothing accepts a site without it.
    body = core[core.index("static bool sdk_at("):core.index("static bool wrapper_at(")]
    first = body.index("if(site<0x53cu || site>n || n-site<0x1b8u || (site&1u)) return false;")
    loop = body.index("for(unsigned i=0;i<sizeof(words)/sizeof(words[0]);i++)\n"
                      "        if(u16(p+site+words[i].offset)!=words[i].value) return false;")
    assert first < loop and "return true" not in body[:loop]
    assert len(re.findall(r"\bsdk_at\(", core)) == 2, "sdk_at has another caller"

    # The discovery's loop: its first site, its last, its step; resolved only through sdk_at.
    discover = core[core.index("const uint8_t *p=image;unsigned found=0,partial=0;"):]
    assert "for(size_t site=0x53cu;site<n && n-site>=0x1b8u;site+=2u) {" in discover
    assert "if(!sdk_at(p,n,base,site,&r)) continue;" in discover
    assert "if(!found) return partial?KUI_VMU_NATIVE_INCOMPLETE:KUI_VMU_NATIVE_NOT_FOUND;" in discover
    assert discover.count("return KUI_VMU_NATIVE_RESOLVED;") == 1

    # The launcher's bounds are those of the loop and the table.
    def number(name):
        value = re.search(r"#define " + name + r" (0x[0-9a-f]+|\d+)u\b", gui)
        assert value, name
        return int(value.group(1), 0)
    assert number("NATIVE_WITNESS_FIRST") == 0x53c and number("NATIVE_WITNESS_TAIL") == 0x1b8
    assert number("NATIVE_WITNESS_BEFORE") == -min(offset for offset, _ in words) == 258
    assert number("NATIVE_WITNESS_AFTER") == max(offset for offset, _ in words) + 2 == 368
    assert number("NATIVE_WITNESS_AFTER") <= number("NATIVE_WITNESS_TAIL")
    assert number("NATIVE_WITNESS_BEFORE") <= number("NATIVE_WITNESS_FIRST")

    # The SDK check starts with that discovery wherever it runs: the layout
    # step needs it resolved, the launcher's and the stage's checks start with
    # the layout step.
    assert layout.count("if(result!=KUI_VMU_NATIVE_RESOLVED || !layout_from_bound(image,n,&fresh,&layout)) return false;") == 1
    build = resolution[resolution.index("static bool discover(const void *image,size_t n,uint32_t load,"):]
    build = build[:build.index("enum kui_vmu_native_heap_result heap=")]
    assert "kui_vmu_native_discover_layout_prefixed(image,n,load,&r->driver,&r->layout,source))return false;" in build
    assert "kui_vmu_native_discover_layout(image,n,load,&r->driver,&r->layout)) return false;" in build
    assert "if(!kui_vmu_native_discover_layout(image,bytes,load,&native_driver,&passive_layout)) {" in stage

    # The combined module's stage: the program is the boot file's bytes or a
    # proved prefix of them, and each of its three builds of the check stops
    # where that discovery does not resolve.
    def flat(text):
        return "".join(text.split())
    retail = flat((ROOT / "src/loader/retail_stage.c").read_text())
    probe = flat((ROOT / "src/loader/vmu_native_heap_probe_stage.c").read_text())
    scope = (ROOT / "src/loader/native_proof_cache_scope.inc").read_text()
    assert "#else" "exec_bytes=manifest.boot_bytes;" in retail
    assert flat("""if(!native_program_extent((const void *)(uintptr_t)KUI_RETAIL_EXEC_ADDRESS,
        exec_bytes,KUI_RETAIL_EXEC_ADDRESS,&program_bytes)) native_report(2u,13u);""") in probe
    assert "exec_bytes=(uint32_t)program_bytes;" in probe
    extent = (ROOT / "src/core/vmu_native_program.c").read_text()
    extent = extent[extent.index("bool kui_vmu_native_program_extent("):]
    assert re.findall(r"\*out=([^;]+);", extent) == ["arena_tail(image,n,load)", "prefix"]
    assert "size_t prefix=n-IP_BYTES;" in extent
    assert flat("""if(!kui_native_stage_cached_discover_image((const void *)(uintptr_t)KUI_RETAIL_EXEC_ADDRESS,
        exec_bytes,KUI_RETAIL_EXEC_ADDRESS,&detail))native_report(2u,detail);""") in probe
    assert ".Ldiscover_proof: .long C(kui_native_stage_discover_image)" in scope
    assert flat("""if(!kui_vmu_native_discover_layout((const void *)(uintptr_t)KUI_RETAIL_EXEC_ADDRESS,
        exec_bytes,KUI_RETAIL_EXEC_ADDRESS,&native_driver,&passive_layout)) native_report(2u,10u);""") in probe
    assert flat("""int driver=kui_vmu_native_discover((const void *)(uintptr_t)KUI_RETAIL_EXEC_ADDRESS,
        exec_bytes,KUI_RETAIL_EXEC_ADDRESS,&native_driver);""") in probe
    assert flat("""else if(driver!=KUI_VMU_NATIVE_RESOLVED || heap!=KUI_VMU_NATIVE_HEAP_FOUND)
        native_report(2u,heap==KUI_VMU_NATIVE_HEAP_FOUND?0u:(uint32_t)heap);""") in probe

    # The same code, whole.
    moved = [(name, path, pinned(path, begin, end)) for name, path, begin, end, digest in PINS
             if pinned(path, begin, end) != digest]
    assert not moved, "pinned SDK-check code changed (see PINS in this file for what to answer first):\n" + \
        "\n".join(f"  {name}: {path} is now {digest}" for name, path, digest in moved)

    # The launcher searches only where it could not hold the program, and only
    # for a combined module whose launch reads the program anyway.
    assert preflight.count("native_cache_scan_large(") == 2
    assert ("return too_large && dual && source_proof?\n"
            "            native_cache_scan_large(image,map,log,cancel,source_proof):NATIVE_CACHE_SKIPPED;") in preflight
    print("PASS native witness: the launcher's 32-word table and four bounds are the Maple discovery's; "
          "sdk_at tests the table first and is the only way to a resolved driver; "
          f"the launcher's and the stage's checks start with that discovery; {len(PINS)} pieces of that code are pinned whole")


if __name__ == "__main__":
    main()
