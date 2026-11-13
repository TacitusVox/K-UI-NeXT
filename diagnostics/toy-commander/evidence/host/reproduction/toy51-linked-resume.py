#!/usr/bin/env python3
"""Replay linked Test 51 controller suffix admission with modeled SCI registers."""

from itertools import product
from pathlib import Path
import importlib.util
import sys

ROOT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location("cut",ROOT/"toy46-linked-first-cut-replay.py")
cut=importlib.util.module_from_spec(spec)
spec.loader.exec_module(cut)
pio=cut.pio
TOKEN=cut.R+0x2b0
IN_IRQ=cut.R+0x2cc
ACQUIRED=cut.R+0x4eb
KEPT=cut.R+0x4ab
COUNT=cut.R+0x4d0
RESULT=0x8c450000
VALUE=0x31415926
BODY=bytes((19+13*i)&255 for i in range(512))


class Replay(cut.Replay):
    def __init__(self,image,area,received,held,later):
        super().__init__(image,selected=False,arm=0,area=area,received=received,
                         slow=False,later=later,reason="overrun",held=held)
        self.lost=received+held
        self.wire_body=bytes(map(pio.reverse,BODY))
        self.write(cut.ACTIVE,1+8+16,1)
        self.write(TOKEN,VALUE)
        self.write(ACQUIRED,1,1)
        self.hardware[pio.RDR]=self.wire_body[received]
        self.store_block(self.area,self.wire_body[:received]+bytes([0x55])*(544-received))
        self.fail_sptr=False

    def read(self,address,count=4):
        value=super().read(address,count)
        return value & ~2 if self.fail_sptr and address==cut.SPTR else value

    def plain(self,op,pc):
        if op&0xf00f==0x200e:  # mulu.w Rm,Rn
            n,m=(op>>8)&15,(op>>4)&15
            self.macl=(self.r[n]&65535)*(self.r[m]&65535)
            self.executing=pc
            self.instructions+=1
        elif op&0xf00f==0x300e:  # addc Rm,Rn
            n,m=(op>>8)&15,(op>>4)&15
            value=self.r[n]+self.r[m]+(self.sr&1)
            self.r[n]=value&0xffffffff
            self.flag(value>0xffffffff)
            self.executing=pc
            self.instructions+=1
        elif op==0x0018:  # sett
            self.flag(True)
            self.executing=pc
            self.instructions+=1
        else:
            super().plain(op,pc)

    def parked(self,later):
        assert self.call(cut.POLL,[later])["result"]==1
        assert self.read(cut.STATE)==4
        assert self.read(cut.POSITION)==pio.LBA
        assert self.read(self.area+516)==VALUE
        assert self.read(self.area+520)==self.lost+1
        assert self.block(self.area,self.lost+1)==self.wire_body[:self.lost]+bytes(1)
        assert tuple(self.hardware[r] for r in (cut.SAR,cut.DAR,cut.TCR,cut.CHCR))==cut.SAVED
        assert self.hardware[pio.PDTR]&0x80

    def suffix(self, context):
        self.write(IN_IRQ,1)
        self.write(cut.ACTIVE,1+8,1)
        self.reg_trace.clear()
        assert self.call(pio.STREAM_FETCH,[pio.LBA,80,0])["result"]==1
        assert self.read(cut.STATE)==4
        assert all(op[0]=="read" for op in self.reg_trace)

        self.write(IN_IRQ,context)
        self.write(cut.ACTIVE,1+8+16,1)
        self.reg_trace.clear()
        assert self.call(pio.STREAM_FETCH,[pio.LBA,80,0])["result"]==0
        assert self.read(cut.STATE)==2
        assert self.read(cut.POSITION)==pio.LBA
        assert self.hardware[cut.TCR]==512-self.lost
        assert self.hardware[cut.DAR]==(self.area&0x1fffffff)+self.lost+1
        assert not self.hardware[pio.PDTR]&0x80
        assert [op[1] for op in self.reg_trace if op[0]=="write"]==[
            pio.PDTR,cut.SPTR,pio.SCR,cut.CHCR,cut.SAR,cut.DAR,cut.TCR,cut.CHCR,pio.SCR]

    def refused(self, reason):
        self.write(IN_IRQ,2)
        lba=pio.LBA
        polled=0
        if reason=="guest_busy":
            self.hardware[cut.CHCR]|=1
        elif reason=="dma_error":
            self.hardware[cut.DMAOR]|=4
        elif reason=="different_lba":
            lba+=1
        elif reason=="different_token":
            self.write(TOKEN,VALUE+1)
        elif reason=="empty_token":
            self.write(TOKEN,0)
        elif reason=="polled":
            polled=1
        elif reason=="empty_offset":
            self.write(self.area+520,0)
        elif reason=="invalid_offset":
            self.write(self.area+520,513)
        elif reason=="unscoped":
            self.write(cut.ACTIVE,1+16,1)
        elif reason=="no_permission":
            self.write(cut.ACTIVE,1+8,1)
        else:
            raise ValueError(reason)

        self.reg_trace.clear()
        state=self.hardware.copy()
        area=self.block(self.area,544)
        assert self.call(pio.STREAM_FETCH,[lba,80,polled])["result"]==1,reason
        assert self.read(cut.STATE)==4,reason
        assert self.read(cut.POSITION)==pio.LBA,reason
        assert self.hardware==state and self.block(self.area,544)==area,reason
        assert all(op[0]=="read" for op in self.reg_trace),reason

    def arm_fault(self):
        self.write(IN_IRQ,2)
        self.fail_sptr=True
        self.reg_trace.clear()
        assert self.call(pio.STREAM_FETCH,[pio.LBA,80,0])["result"]==1
        assert self.read(cut.STATE)==3
        assert self.read(cut.POSITION)==pio.LBA
        assert self.hardware[pio.PDTR]&0x80
        assert tuple(self.hardware[r] for r in (cut.SAR,cut.DAR,cut.TCR,cut.CHCR))==cut.SAVED
        assert [op[1] for op in self.reg_trace if op[0]=="write"]==[pio.PDTR,cut.SPTR,pio.PDTR]

    def receive(self,kind):
        high,low=pio.crc16(BODY).to_bytes(2,"big")
        if kind=="bad_syndrome":
            high,low=high,0xff
        self.store_block(self.area+self.lost+1,self.wire_body[self.lost+1:]+bytes([pio.reverse(high)]))
        if kind=="second_cut":
            self.hardware.update({cut.TCR:3, cut.DAR:(self.area&0x1fffffff)+510,
                                  cut.CHCR:0x4911, pio.RDR:pio.reverse(low), pio.SSR:0xe4})
            assert self.call(cut.POLL,[1])["result"]==6
            assert self.read(cut.STATE)==3
            assert self.read(cut.POSITION)==pio.LBA
            assert self.read(COUNT)==0
            return
        self.hardware.update({cut.TCR:0,cut.DAR:(self.area&0x1fffffff)+513,
                              cut.CHCR:0x4913,pio.RDR:pio.reverse(low),pio.SSR:0xe4})
        assert self.call(cut.POLL,[1])["result"]==0
        assert self.read(cut.STATE)==1
        assert self.read(cut.POSITION)==pio.LBA+1
        assert tuple(self.hardware[r] for r in (cut.SAR,cut.DAR,cut.TCR,cut.CHCR))==cut.SAVED
        if kind=="unscoped":
            self.write(cut.ACTIVE,1,1)
        if kind=="changed_token":
            self.write(TOKEN,VALUE+1)
        result=self.call(pio.STREAM_TAKE,[pio.LBA,RESULT])["result"]
        if kind=="valid":
            assert result==self.area
            assert self.read(RESULT)==0
            assert self.block(self.area,len(BODY))==BODY
            assert self.read(COUNT)==1
        else:
            assert result==0
            assert self.read(RESULT)==5
            assert self.read(COUNT)==0
        assert self.read(KEPT,1)==0
        assert self.call(pio.STREAM_TAKE,[pio.LBA,RESULT])["result"]==0


def main():
    for arg in sys.argv[1:]:
        path=Path(arg)
        count=0
        for area,later,context,kind in product(range(2),range(2),(0,2),
                ("valid","second_cut","bad_syndrome","unscoped","changed_token")):
            for received,held in ((1,0),(1,1),(256,0),(256,1),(511,0)):
                cpu=Replay(path,area,received,held,later)
                cpu.parked(later)
                cpu.suffix(context)
                cpu.receive(kind)
                count+=1
        refused=0
        for area,reason in product(range(2),("guest_busy","dma_error","different_lba",
                "different_token","empty_token","polled","empty_offset","invalid_offset",
                "unscoped","no_permission")):
            cpu=Replay(path,area,256,1,True)
            cpu.parked(True)
            cpu.refused(reason)
            refused+=1
        for area in range(2):
            cpu=Replay(path,area,256,1,True)
            cpu.parked(True)
            cpu.arm_fault()
        print(f"{path}: {count} linked SH-4 foreground/controller suffix cases, "
              f"{refused} controller refusals with no peripheral writes, and two re-arm faults")


if __name__=="__main__":
    main()
