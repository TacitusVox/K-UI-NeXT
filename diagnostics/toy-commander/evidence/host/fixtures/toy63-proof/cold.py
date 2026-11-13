"""Replay the actual physical-disc stage installer on original Toy bytes."""
import argparse
from hashlib import sha256
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / "trace-reader/tools"), str(ROOT / "trace-reader/tests")]
from check_vmu_native_worker_gateway import read_elf, symbol_bytes
from profile_vmu_native_release_sh import ReleaseSH
from runtime_package import verify
from sh4_delay_slots import validate_delay_slot
from test_toy_original_gd_poll import SOURCE_SHA256, MASK, signed

PUBLISH = {
    0x8c04e9b4: ('heap_guard', None), 0x8c027a24: ('finish', 0x8c027104),
    0x8c06d428: ('exit', 0x8c08d638), 0x8c04c774: ('render', 0x8c04c238),
    0x8c04c568: ('scene_slot', 0x8c0fc464), 0x8c0000bc: ('proxy', 0x8c007000),
}
LEASE = 0x8cfd0000
UNCACHED = 0x20000000
HEAP = 0x8c124340
STOP = 0x8c008a00
STACK = 0x8cff0000
CALL = 0x8c00c100
ALLOC = 0x8c0b3ede
EXPORTS = ('proxy','finish','config','binary_end','bss_end','heap_guard','exit','render','scene_slot','scene')


class Refused(Exception):
    pass


class Installer(ReleaseSH):
    def __init__(self, build, image, *, writable):
        super().__init__(build / "stage.elf")
        self.store_block(0x8c010000, image)
        self.cache_publications = []
        self.allocations = []
        self.copy_block = []
        self.blob = self.symbols["__disc_toy_worker_begin"][0]
        blob_end = self.symbols["__disc_toy_worker_end"][0]
        self.worker = self.block(self.blob, blob_end - self.blob)
        assert self.worker == (build / "toy-trace.bin").read_bytes()
        self.exports = dict(zip(EXPORTS, struct.unpack_from("<10I",self.worker)))
        for address, (_, original) in PUBLISH.items():
            if original is None:
                continue
            if address == 0x8c0000bc:
                self.write(address, original)
            else:
                assert self.read(address) == original, hex(address)
        self.original = {address: self.read(address) for address in PUBLISH}
        self.write(self.symbols["_disc_toy_firmware"][0], self.original[0x8c0000bc])
        for address, value in ((0x8c117dbc,0x8c0c5ac8),(0x8c0c5ac8,0x8c0b3ef2),
                               (0x8c0c5acc,0x8c0b3fc0),(0x8c117de0,0x8c117dc0),
                               (0x8c117dc0,HEAP),(HEAP,0x8c117dc0),(HEAP+4,0x76de6)):
            self.write(address,value)
        self.write(CALL, 0x8c04e924)
        self.write(CALL+4, HEAP)
        self.write(CALL+8, 0xedbcc0)
        self.write(CALL+12, 0x8c00c100)
        self.range_seed = self.symbols["__disc_toy_range_seed"][0]
        assert self.block(self.range_seed,40) == b"KUITFR63"+bytes(32)
        self.ranges = struct.pack("<8I",0x69000,0x1100,0x6a000,0x1000,
                                  0x6e000,0x7000,0x77000,0x6000)
        self.store_block(self.range_seed+8,self.ranges)
        self.seed = self.symbols["__disc_toy_report_seed"][0]
        assert self.block(self.seed,64) == b"KUITRS01"+struct.pack("<II",1,64)+bytes(48)
        if writable:
            self.write(self.seed+16,63)
            self.write(self.seed+24,0x117)
            self.write(self.seed+28,0x245)
        self.writes.clear()

    def plain(self, op, pc):
        n = (op >> 8) & 15
        if op & 0xf0ff == 0x0002:  # stc sr,Rn
            self.r[n] = self.sr
        elif op & 0xf0ff == 0x00a3:  # ocbp @Rn
            assert not self.r[n] & 31
            self.cache_publications.append(self.r[n])
        elif op & 0xff00 == 0xc900:  # and #imm,R0
            self.r[0] &= op & 255
        else:
            return super().plain(op, pc)
        self.executing=pc
        self.instructions+=1

    def stub(self, pc):
        destination, source, count = self.r[4:7]
        if pc == ALLOC:
            assert destination == 0x30000
            self.allocations.append(destination)
            self.write(HEAP+4,0x755e5)
            self.write(LEASE-28,0x1801)
            self.r[0] = LEASE
        elif pc == self.symbols["_memcpy"][0]:
            self.copy_block.append((destination,source,count))
            self.store_block(destination,self.block(source,count))
            self.r[0] = destination
        elif pc == self.symbols["_memset"][0]:
            assert destination==LEASE|UNCACHED and not source and count == self.exports['bss_end']-destination
            self.store_block(destination,bytes(count))
            self.r[0] = destination
        elif pc == self.symbols["_memcmp"][0]:
            assert destination==LEASE|UNCACHED and source==self.blob and count==len(self.worker)
            self.r[0] = int(self.block(destination,count)!=self.block(source,count))
        elif pc == self.symbols["_stopped"][0]:
            message=bytes(self.read(destination+i,1) for i in range(40)).split(b"\0")[0]
            raise Refused(message)
        else:
            return False
        self.pc = self.pr
        return True

    def install(self):
        self.pc=self.symbols["_kui_disc_toy_install"][0]
        self.r[4], self.r[15], self.pr = CALL, STACK, STOP
        saved = self.r[8:16]
        for _ in range(10000):
            if self.pc==STOP:
                assert self.r[8:16]==saved and self.sr & ~1 == 0x600000f0
                return
            if self.stub(self.pc):
                continue
            pc=self.pc
            op=self.read(pc,2)
            target, delay = pc+2, False
            if op >> 12 in (0xa,0xb):
                target, delay = pc+4+2*signed(op&0xfff,12), True
                if op >> 12 == 0xb:
                    self.pr=pc+4
            elif op & 0xf0ff in (0x400b,0x402b):
                target, delay = self.r[(op>>8)&15], True
                if op & 0xf0ff == 0x400b:
                    self.pr=pc+4
            elif op == 0x000b:
                target, delay = self.pr, True
            elif op >> 8 in (0x89,0x8b,0x8d,0x8f):
                delay = op >> 8 in (0x8d,0x8f)
                if bool(self.sr&1) == (op >> 8 in (0x89,0x8d)):
                    target=pc+4+2*signed(op&255,8)
                elif delay:
                    target=pc+4
            else:
                self.plain(op,pc)
            if delay:
                slot=self.read(pc+2,2)
                validate_delay_slot(slot,pc)
                self.plain(slot,pc+2)
            self.pc=target&MASK
        raise AssertionError("physical installer exceeded exact linked instruction bound")

    def check(self):
        self.install()
        assert self.allocations==[0x30000]
        assert (LEASE|UNCACHED,self.blob,len(self.worker)) in self.copy_block
        assert self.block(LEASE|UNCACHED,len(self.worker))==self.worker
        cfg=self.exports['config']
        assert self.read(cfg)==self.original[0x8c0000bc]
        assert self.block(cfg+60,32)==self.ranges
        assert self.block(cfg+92,64)==self.block(self.seed,64)
        patches=[(address,value) for _,address,value,width in self.writes
                 if width==4 and address in PUBLISH]
        assert patches==[(address,self.exports[name]) for address,(name,_) in PUBLISH.items()]
        for address,(name,_) in PUBLISH.items():
            assert self.read(address|UNCACHED)==self.exports[name]
            assert address&~31 in self.cache_publications
        assert self.read(self.exports['scene_slot'])==self.exports['scene']


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('original',type=Path)
    ap.add_argument('build',type=Path)
    args=ap.parse_args()
    image=args.original.read_bytes()
    assert sha256(image).hexdigest()==SOURCE_SHA256
    package=verify((args.build/'native-toy-trace.kui').read_bytes())
    worker=read_elf(args.build/'toy-trace.elf')
    for name in ('exit','render','scene','scene_slot'):
        symbol={'exit':'_kui_toy_movie_exit_hook','render':'_kui_toy_movie_render_hook',
                'scene':'_kui_toy_movie_present_hook','scene_slot':'_kui_toy_movie_present_slot'}[name]
        address, _, section = symbol_bytes(worker,symbol,1 if name=='scene_slot' else 2)
        assert section[2]&7 == (2 if name=='scene_slot' else 6)
        assert address&0xe0000000 == 0xa0000000
    for writable in (False,True):
        Installer(args.build,image,writable=writable).check()
    for index,value in ((6,0x8cfd0100),(7,0xacfd0003),(8,0xacfd0102),(9,0xacfd0000)):
        cpu=Installer(args.build,image,writable=True)
        cpu.write(cpu.blob+index*4,value)
        try:
            cpu.install()
        except Refused as error:
            assert error.args == (b'TOY TRACE WORKER LAYOUT REFUSED',)
        else:
            raise AssertionError(f"accepted invalid export {index}")
        assert not cpu.allocations
        assert all(cpu.read(address)==value for address,value in cpu.original.items())
    cpu=Installer(args.build,image,writable=True)
    cpu.write(cpu.blob+cpu.exports['scene_slot']-(LEASE|UNCACHED),0xacfd002a)
    try:
        cpu.install()
    except Refused as error:
        assert error.args == (b'TOY SCENE HOOK REFUSED',)
    else:
        raise AssertionError("accepted invalid initialized scene cell")
    assert all(cpu.read(address)==value for address,value in cpu.original.items())
    print(f"Physical Test 63 {package['build']}: exact cold installer and six cache-published"
          " pointers, two report modes and five corrupted exports")


if __name__=='__main__':
    main()
