"""Run existing linked startup proofs using the Toy reader's known firmware bounds."""
from pathlib import Path
import sys
import inspect
from json import dumps
import struct

ROOT = Path(__file__).resolve().parent
sys.path[:0] = [str(ROOT/'trace-reader/tools'), str(ROOT/'trace-reader/tests')]
import check_vmu_native_stage_stack as cold
import check_native_dual_callbacks as callbacks
import check_vmu_native_sdk_sci_route as sdk
from prove_native_sdk_sci_structural_runtime import original_dispatch
from check_vmu_native_worker_gateway import read_elf
from runtime_package import verify,flatten_elf
from check_vmu_native_sdk_sci_route import initialized_at

OWNER=ROOT/'toy34-inputs/K-UI/ToyCommander-1GUTH.BIN'
BOTTOM,TOP=0x8c007e80,0x8c007fe0

def main(build,mode):
    images=[read_elf(build/name) for name in ('stage.elf','resident-sci.elf','native-worker.elf','entry.elf')]
    stage,resident,worker,entry=images
    assert resident[4]['__retail_hook_stack_bottom'][0]==BOTTOM
    assert resident[4]['__retail_hook_stack'][0]==TOP
    old=cold.inspect_resident
    cold.inspect_resident=lambda image,limit:old(image,max(limit,BOTTOM))
    # Earlier standard fixtures encode a narrower region. Change only their
    # upper bound to the independently pinned existing Toy firmware limit.
    source=inspect.getsource(callbacks.expected_entries)
    assert source.count('<= 0x8c007ba0')==1
    exec(source.replace('<= 0x8c007ba0', '<= '+hex(BOTTOM)),vars(callbacks))
    sdk.GUARD=BOTTOM;sdk.PRIVATE_TOP=TOP-16
    stage_audit=cold.check(stage,resident,worker=worker)
    dispatch=original_dispatch(resident,OWNER)
    stage_bin=(build/'stage.bin').read_bytes()
    low_bin=(build/'resident-sci.bin').read_bytes()
    pkg=(build/f'retail-toy-test-{mode}.kui').read_bytes()
    payload,size=flatten_elf(entry[0])
    header=verify(pkg)
    assert payload==pkg[64:]
    assert header['memory_bytes']==size
    boot_map=entry[4]['__retail_map'][0]
    assert boot_map==0x8c011000
    boot_start,boot_end=(entry[4][name][0] for name in ('__retail_stage_blob_start','__retail_stage_blob_end'))
    assert boot_start==0x8c012000
    assert boot_end-boot_start==len(stage_bin)
    assert initialized_at(entry,boot_start,len(stage_bin))==stage_bin
    assert struct.unpack_from('<I',payload,0x2c)==(len(stage_bin),)
    low_begin,low_end=(stage[4][name][0] for name in ('__retail_resident_sci_blob_start','__retail_resident_sci_blob_end'))
    assert low_end-low_begin==len(low_bin)
    assert initialized_at(stage,low_begin,len(low_bin))==low_bin
    proof=dict(stage=stage_audit,original_sdk=dispatch,package=header,
        entry_byte_copy=len(stage_bin),stage_sci_blob_bytes=len(low_bin))
    out=ROOT/f'toy{mode}-startup-linked-{build.name}.json'
    out.write_text(dumps(proof,indent=2)+'\n')
    print(f'Test {mode}: {stage_audit["actual_relay_publication_cases"]} relay, '
          f'{stage_audit["owner_stack_boundary_execution_cases"]} owner-stack, '
          f'{dispatch["actual_sdk_integer_dispatch_to_linked_callback_cases"]} original SCI callback cases. {out}')

if __name__=='__main__':
    main(Path(sys.argv[1]),int(sys.argv[2]))
