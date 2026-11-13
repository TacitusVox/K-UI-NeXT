#!/usr/bin/env python3
"""Check the exact bounded Toy reader, auxiliary layout, and stack switch."""
from pathlib import Path
import json
import struct
import sys

SOURCE = Path(__file__).resolve().parent / 'trace-reader'
sys.path.insert(0, str(SOURCE / 'tools'))

import check_retail_stack as graph
import check_vmu_native_background as background
import check_vmu_native_readahead as readahead
from check_loader_layout import inspect_elf
from check_vmu_native_worker_gateway import read_elf, symbol_bytes


def resident_stack(directory, stem):
    path = directory / f'{stem}.elf'
    image = read_elf(path)
    bss_end = image[4]['__retail_resident_bss_end'][0]
    bottom = image[4]['__retail_hook_stack_bottom'][0]
    top = image[4]['__retail_hook_stack'][0]
    expected = (0x8c007e80, 0x8c007fe0) if stem == 'resident-sci' else (background.BOTTOM, background.TOP)
    assert bss_end <= bottom and (bottom, top) == expected
    assert top - bottom == 352 and top < 0x8c008000
    if stem == 'resident-sci':
        assert image[4]['__kui_toy_slow_retry_stack_bottom'][0] == bottom
        assert image[4]['__kui_toy_slow_retry_stack_top'][0] == top
        assert image[4]['_kui_vmu_observe_sd_lease'][0] < 0x8c007800
        assert image[4]['_kui_retail_sdk_sci_entry'][0] < 0x8c007ba0
        address, body, section = symbol_bytes(image, '_finish', 2)
        assert section[2] & 7 == 6 and 0x8c007800 <= address < address + len(body) <= bottom
    payload = inspect_elf(image[0], background.LOW, bottom)['payload']
    assert payload == (directory / f'{stem}.bin').read_bytes()
    helper_names = [name for name, info in image[4].items() if info[2] == 2 and name.startswith('___')]
    assert helper_names == ['___udivsi3'], helper_names
    helper_address, helper_size = background.typed(image, '___udivsi3', 2)
    library = background.library_cfg(background.rows(background.disassemble(path)), helper_address, helper_size)
    reports = sorted(directory.glob(f'{stem}.elf.ltrans*.ci'))
    assert reports and all(readahead.closed_callgraph(report.read_text()) for report in reports)
    frames, calls = graph.load(reports, qualified=True)
    roots = dict(background.ROOTS)
    switches = {}
    if stem == 'resident-sci':
        roots['kui_retail_sdk_sci_dispatch'] = 0
        switches[('native_readahead_visit', 'retail_async.c')] = 20
        switches[('deliver', 'retail_async.c')] = 20
    paths, callbacks, escapes = background.stack_paths(frames, calls, roots, switches)
    worst = max(path['bytes'] for path in paths.values())
    total = worst + library['stack_bytes'] + background.EXTRA_MARGIN + background.GUARD + background.TOP_GAP
    assert total <= top - bottom, (total, top-bottom, paths)
    return dict(bss_end=hex(bss_end), stack_bottom=hex(bottom), stack_top=hex(top),
                total_bytes=total, available_bytes=top-bottom, paths=paths,
                callbacks=len(callbacks), escapes=len(escapes)), image


def auxiliary_layout(directory, aux, resident):
    binary = (directory / 'native-ra-aux.bin').read_bytes()
    wire = (directory / 'native-ra-aux.reloc').read_bytes()
    assert readahead.generate(aux[0], binary, readahead.AUX, readahead.SPAN) == wire
    loads = readahead.validate_loads(aux[0], binary, readahead.AUX, readahead.SPAN)
    address, header, section = symbol_bytes(aux, '_kui_native_readahead_aux_exports', 1)
    context = aux[4]['_kui_native_readahead_aux_context']
    visit = aux[4]['_kui_native_readahead_aux_visit'][0]
    init = aux[4]['_kui_native_readahead_aux_init'][0]
    assert address == readahead.AUX and section[2] & 7 == 2
    hook, hook_body, hook_section = symbol_bytes(aux, '_kui_toy_movie_exit_hook', 2)
    assert hook_section[2] & 7 == 6 and hook not in (visit, init)
    assert readahead.AUX+0x1000 <= hook < hook + len(hook_body) <= readahead.AUX+len(binary)
    chunk, chunk_body, chunk_section = symbol_bytes(aux, '_kui_toy_movie_chunk_hook', 2)
    assert chunk_section[2] & 7 == 6 and chunk not in (visit, init, hook)
    assert readahead.AUX+0x1000 <= chunk < chunk + len(chunk_body) <= readahead.AUX+len(binary)
    refill, refill_body, refill_section = symbol_bytes(aux, '_kui_toy_movie_refill_hook', 2)
    assert refill_section[2] & 7 == 6 and refill not in (visit, init, hook, chunk)
    assert readahead.AUX+0x1000 <= refill < refill+len(refill_body) <= readahead.AUX+len(binary)
    present, present_body, present_section = symbol_bytes(aux, '_kui_toy_movie_present_hook', 2)
    assert present_section[2] & 7 == 6 and present not in (visit, init, hook, chunk, refill)
    assert readahead.AUX+0x1000 <= present < present+len(present_body) <= readahead.AUX+len(binary)
    slot, slot_body, slot_section = symbol_bytes(aux, '_kui_toy_movie_present_slot', 1)
    assert slot_section[2] & 7 == 2 and len(slot_body) == 4 and not slot & 3
    assert slot_body == struct.pack('<I', present)
    assert slot >= readahead.AUX+0x1000 and slot+len(slot_body) <= readahead.AUX+len(binary)
    render, render_body, render_section = symbol_bytes(aux, '_kui_toy_movie_render_hook', 2)
    assert render_section[2] & 7 == 6 and render not in (visit, init, hook, chunk, refill, present)
    assert readahead.AUX+0x1000 <= render < render+len(render_body) <= readahead.AUX+len(binary)
    assert header == struct.pack('<16I', 0x4b524131, 1, 64, visit, init, context[0],
        readahead.AUX+len(binary), readahead.STACK_BOTTOM, readahead.STACK_TOP,
        readahead.STACK_TOP, hook, chunk, refill, slot, render, 0)
    assert context[2] == 1 and not context[0] & 31 and 0xac421040 <= context[0] < context[0]+context[1] <= 0xac422000
    assert aux[3][context[3]][2] & 7 == 3
    assert not any(readahead.initialized_at(aux, context[0], context[1]))
    assert all(readahead.AUX+0x1000 <= at < readahead.AUX+len(binary) for at in (visit, init))
    for name,(at,count,_,owner) in aux[4].items():
        if count and owner < len(aux[3]) and aux[3][owner][2] & 3 == 3:
            assert at & 0xe0000000 == 0xa0000000, name
    return dict(context=hex(context[0]), context_bytes=context[1], stack_bottom=hex(readahead.STACK_BOTTOM),
                stack_top=hex(readahead.STACK_TOP), typed_low_ops=readahead.typed_ops(resident),
                refill_hook=hex(refill), present_slot=hex(slot), present_hook=hex(present),
                render_hook=hex(render),
                physical_loads=loads)


def guest_probe_stack(directory):
    """The retained guest hooks call bounded C directly on the game stack."""
    reports = sorted((directory / 'ra').rglob('*.ci'))
    frames, calls = graph.load(reports, qualified=True)
    aux_path = directory / 'native-ra-aux.elf'
    aux = read_elf(aux_path)
    division_address, division_size = background.typed(aux, '___udivsi3', 2)
    division = background.library_cfg(
        background.rows(background.disassemble(aux_path)), division_address, division_size,
    )

    def depth(node, ancestors):
        assert node not in ancestors and node in frames
        targets = [target for target, _ in calls.get(node, ())]
        assert '__indirect_call' not in targets
        return frames[node] + max((depth(target, (*ancestors,node)) for target in targets),default=0)

    probes = {
        'kui_toy_movie_chunk_observe': 16,
        'kui_toy_movie_refill_defer': 24,
        'kui_toy_movie_render_enter': 16,
        'kui_toy_movie_render_leave': 16,
        'kui_toy_movie_scene_observe': 32,
        'kui_toy_movie_present_boundary': 16,
    }
    result = {}
    for name, assembly in probes.items():
        c_bytes = depth(name, ())
        helper_bytes = division['stack_bytes'] if name == 'kui_toy_movie_scene_observe' else 0
        total = c_bytes + assembly + helper_bytes
        assert 0 < total <= 192, (name, c_bytes, assembly, helper_bytes)
        result[name] = dict(c_bytes=c_bytes, assembly_bytes=assembly,
                            library_helper_bytes=helper_bytes, total=total)
    return result


def main():
    directory = Path(sys.argv[1]).resolve()
    mode, = [int(path.stem.rsplit('-',1)[1]) for path in directory.glob('retail-toy-test-*.kui')]
    assert mode == 65
    readahead.SPAN = 0x5000
    readahead.STACK_BOTTOM, readahead.STACK_TOP = 0xac425000, 0xac426000
    sci, resident = resident_stack(directory, 'resident-sci')
    legacy, _ = resident_stack(directory, 'resident-legacy')
    aux = read_elf(directory / 'native-ra-aux.elf')
    readahead.GUARD = int(sci['stack_bottom'], 16)
    print(json.dumps({'resident-sci':sci, 'resident-legacy':legacy,
        'auxiliary_layout':auxiliary_layout(directory, aux, resident),
        'auxiliary_stack':readahead.private_stack(directory, resident, self_test=True),
        'auxiliary_gateway':readahead.gateway_cases(resident, aux),
        'guest_probe_stack':guest_probe_stack(directory)}, indent=2))


if __name__ == '__main__':
    main()
