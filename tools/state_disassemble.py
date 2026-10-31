"""Read-only i960 ROM inspection using the pinned MAME disassembler tables.
No copyrighted game bytes are stored in this tool. Outputs belong in private research.
"""
from pathlib import Path
import argparse,re,struct,zipfile
REGS=['pfp','sp','rip']+[f'r{i}' for i in range(3,16)]+[f'g{i}' for i in range(15)]+['fp']
def program(archive):
    out=bytearray(0x200000)
    with zipfile.ZipFile(archive) as z:
        for name,base in [('epr-18524.12',0),('epr-18525.13',2),('epr-18518.14',0x100000),('epr-18519.15',0x100002)]:
            b=z.read(name)
            out[base:base+len(b)*2:4]=b[0::2];out[base+1:base+len(b)*2:4]=b[1::2]
    return out
class Decoder:
    def __init__(self,source,data):
        s=Path(source).read_text();self.data=data
        tables=s.split('const i960_disassembler::mnemonic_t ')
        self.main=[(n,int(t),int(f)) for n,t,f in re.findall(r'\{ "([^"]+)", (\d+), (-?\d+) \}',tables[1])]
        self.reg={int(t,16):(n,int(f)) for n,t,f in re.findall(r'\{ "([^"]+)", 0x([0-9a-f]+), (-?\d+) \}',tables[2])}
    def read(self,a):return struct.unpack_from('<I',self.data,a)[0]
    def decode(self,a):
        w=self.read(a);n,t,c=self.main[w>>24];sz=4;ops=[]
        sx=lambda v,b:(v^(1<<(b-1)))-(1<<(b-1))
        reg=lambda x:REGS[x&31]
        if t==1:
            if c:ops=[hex((a+sx(w&0xfffffc,24))&0xffffffff)]
        elif t==2:
            r1=(w>>19)&31;r2=(w>>14)&31
            ops=[reg(r1)] if c==1 else [str(r1) if w&0x2000 else reg(r1),reg(r2),hex(a+sx(w&0x1ffc,13))]
        elif t==3:
            rd=reg(w>>19);ab=reg(w>>14)
            if w&0x1000:
                mode=(w>>10)&15;sc=1<<((w>>7)&7);ix=reg(w);d=self.read(a+4)
                sz=8 if mode==5 or mode>=12 else 4
                ef={4:f'({ab})',5:hex((a+d+8)&0xffffffff),7:f'({ab})[{ix}*{sc}]',12:hex(d),13:f'{hex(d)}({ab})',14:f'{hex(d)}[{ix}*{sc}]',15:f'{hex(d)}({ab})[{ix}*{sc}]'}.get(mode,'?')
            else:ef=hex(w&0xfff)+(f'({ab})' if w&0x2000 else '')
            ops=[ef] if c==1 else [ef,rd] if c==2 else [rd,ef]
        elif t==4:
            n,c=self.reg.get(((w>>20)&0xff0)|((w>>7)&15),('?',0))
            r1=w&31;r2=(w>>14)&31;rd=(w>>19)&31
            v1=str(r1) if w&0x800 else reg(r1);v2=str(r2) if w&0x1000 else reg(r2);dst=reg(rd)
            if c in (1,10):ops=[v1]
            elif c==-1:ops=[dst]
            elif c in (2,20):ops=[v1,v2]
            elif c in (-2,-20):ops=[v1,dst]
            elif c:ops=[v1,v2,dst]
        else:n='?';ops=[hex(w)]
        return sz,f'{a:08x} {w:08x} {n:9}'+','.join(ops)
    def listing(self,start,end):
        a=start
        while a<end:
            size,line=self.decode(a);yield line;a+=size
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--start',type=lambda v:int(v,0),default=0);p.add_argument('--end',type=lambda v:int(v,0),default=0x200000);p.add_argument('--output',type=Path)
    a=p.parse_args();d=Decoder(a.source,program(a.rom));s='\n'.join(d.listing(a.start,a.end))+'\n'
    if a.output:a.output.write_text(s)
    else:print(s,end='')
