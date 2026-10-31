"""Validate the original-game state investigation without guessed RAM semantics."""
from pathlib import Path
import argparse,hashlib,json,re,struct
from state_disassemble import program
FINGERPRINTS={0x2720:0x80183000,0x2724:0x500032,0x19954:0x8c883000,0x19958:0x507260,0x19988:0x8c883000,0x1998c:0x507264,0x19b6c:0x59210981,0x19b70:0x92245000,0x37c30:0x90703000,0x37c34:0x50a2a4,0x37c7c:0x90703000,0x37c80:0x50a2a8,0x530b4:0x82a83000,0x530b8:0x5d051d,0x5313c:0x80a03000,0x53140:0x5d051d}
def validate(rom,observe,trace,service):
    code=program(rom)
    for address,value in FINGERPRINTS.items():
        if struct.unpack_from('<I',code,address)[0]!=value:raise ValueError(f'Code fingerprint failed {address:08x}')
    def state(tick):
        b=(Path(observe)/f'ram-{tick:04}.bin').read_bytes()
        read=lambda a:struct.unpack_from('<I',b,a-0x500000)[0]
        return {'time':tick/10,'mode':b[0x32],'submode':b[0x38],'player_state':[read(0x502088+i*4) for i in range(2)],'lives':[read(0x502090+i*4) for i in range(2)],'ammo':[read(0x507260+i*4) for i in range(2)],'capacity':[read(0x507268+i*4) for i in range(2)],'weapon':[read(0x507270+i*4) for i in range(2)],'accepted_shots':[read(0x50a2a4+i*4) for i in range(2)]}
    timeline=[state(t) for t in [150,170,310,345,365,385,395,415,445]]
    assert timeline[0]['mode']==5 and timeline[1]['mode']==8
    assert timeline[2]['player_state']==[1,1]
    assert timeline[3]['ammo']==[5,6] and timeline[3]['accepted_shots']==[1,0]
    assert timeline[5]['ammo']==[1,3] and timeline[5]['accepted_shots']==[5,3]
    assert timeline[6]['ammo']==[6,2] and timeline[6]['accepted_shots']==[5,4]
    assert timeline[7]['ammo']==[4,0] and timeline[7]['accepted_shots']==[7,6]
    assert timeline[8]['accepted_shots']==[10,6]
    events=[]
    for line in (Path(trace)/'events.txt').read_text().splitlines():
        m=re.fullmatch(r'WRITE ([0-9.]+) IP ([0-9a-f]+) ADDRESS ([0-9a-f]+) DATA ([0-9a-f]+) MASK ([0-9a-f]+)',line)
        if m:events.append((float(m[1]),*(int(m[i],16) for i in range(2,6))))
    decrements=[e for e in events if e[1]==0x19b74 and e[2] in [0x507260,0x507264]]
    statistics=[e for e in events if e[1] in [0x37c44,0x37c90] and e[2] in [0x50a2a4,0x50a2a8]]
    assert len(decrements)==17 and len(statistics)==17
    assert len([e for e in decrements if e[2]==0x507260])==11
    reloads=[e for e in events if e[1]==0x19b24 and e[2]==0x507260 and e[3]==6]
    assert len(reloads)==1 and 39<reloads[0][0]<40
    b=(Path(service)/'workram-025.bin').read_bytes();assert b[0xd051d]==1 and b[0xd0518]==9
    return {'program_sha256':hashlib.sha256(code).hexdigest(),'fingerprints':len(FINGERPRINTS),'timeline':timeline,'native_write_trace':{'ammo_decrements':len(decrements),'accepted_stat_increments':len(statistics),'p1':11,'p2':6,'p1_reload_time':reloads[0][0],'note':'Tap IP is the instruction after the store: original ammo store19b70, statistics37c3c/37c88.'},'service':{'active':1,'menu':9,'cursor':b[0xd051c],'phase':struct.unpack_from('<I',b,0xd0654)[0]}}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['rom','observe','trace','service','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();r=validate(a.rom,a.observe,a.trace,a.service);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r['native_write_trace']))
