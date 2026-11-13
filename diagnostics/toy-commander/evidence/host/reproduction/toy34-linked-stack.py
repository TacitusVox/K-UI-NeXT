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
    assert bss_end <= bottom == background.BOTTOM and top == background.TOP
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
    assert header == struct.pack('<16I', 0x4b524131, 1, 64, visit, init, context[0],
        readahead.AUX+len(binary), readahead.STACK_BOTTOM, readahead.STACK_TOP,
        readahead.STACK_TOP, 0, 0, 0, 0, 0, 0)
    assert context[2] == 1 and not context[0] & 31 and 0xac421040 <= context[0] < context[0]+context[1] <= 0xac422000
    assert aux[3][context[3]][2] & 7 == 3
    assert not any(readahead.initialized_at(aux, context[0], context[1]))
    assert all(readahead.AUX+0x1000 <= at < readahead.AUX+len(binary) for at in (visit, init))
    for name,(at,count,_,owner) in aux[4].items():
        if count and owner < len(aux[3]) and aux[3][owner][2] & 3 == 3:
            assert at & 0xe0000000 == 0xa0000000, name
    return dict(context=hex(context[0]), context_bytes=context[1], stack_bottom=hex(readahead.STACK_BOTTOM),
                stack_top=hex(readahead.STACK_TOP), typed_low_ops=readahead.typed_ops(resident), physical_loads=loads)


def main():
    directory = Path(sys.argv[1]).resolve()
    sci, resident = resident_stack(directory, 'resident-sci')
    legacy, _ = resident_stack(directory, 'resident-legacy')
    aux = read_elf(directory / 'native-ra-aux.elf')
    print(json.dumps({'resident-sci':sci, 'resident-legacy':legacy,
        'auxiliary_layout':auxiliary_layout(directory, aux, resident),
        'auxiliary_stack':readahead.private_stack(directory, resident, self_test=True),
        'auxiliary_gateway':readahead.gateway_cases(resident, aux)}, indent=2))


if __name__ == '__main__':
    main()
