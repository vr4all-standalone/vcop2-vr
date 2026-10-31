"""Privately extract source-identified original VC2 gun meshes from owned media.
The runtime model table must be observed from the unmodified original game.
No game data is embedded in this tool or written to the source tree.
"""
import argparse,json,math,struct,hashlib,zipfile
from pathlib import Path
from state_disassemble import program
NAMES={1:'scene-pistol',2:'rifle',3:'automatic',4:'machine-gun',5:'shot-gun',6:'magnum'}
MODELS={1:0x143,2:0x116,3:0x115,4:0x19a,5:0x19b,6:0x199}
def region(archive,names):
    with zipfile.ZipFile(archive) as z:
        a,b=[z.read(n) for n in names]
        if len(a)!=len(b):raise ValueError('Mismatched interleaved ROM size')
        result=bytearray(len(a)*2);result[0::4]=a[0::2];result[1::4]=a[1::2];result[2::4]=b[0::2];result[3::4]=b[1::2]
        return result

def decode(table,polygons,textures,model):
    tp,th,oba,count=struct.unpack_from('<4I',table,model*16)
    if oba&0x1800000!=0x800000 or tp&0x800000 or th&0x800000:raise ValueError('Not ROM-backed model')
    at=(oba&0x7fffff)*4
    point=lambda a:struct.unpack_from('<3f',polygons,a)
    p0,p1=point(at),point(at+12);at+=24;faces=[]
    for _ in range(min(count or 0xfffff,4096)):
        attr,=struct.unpack_from('<I',polygons,at);at+=4
        if not attr&3:return dict(model=model,object=oba,texture_point=tp,texture_header=th,faces=faces)
        normal,p2,p3=point(at),point(at+12),point(at+24);at+=36
        n=4 if attr&1 else 3
        if n==3:p3=p2
        points=[p1,p0,p2,p3][:n]
        if not all(math.isfinite(v) and abs(v)<1e6 for p in points for v in p):raise ValueError('Invalid model coordinate')
        uv=[struct.unpack_from('<2H',textures,(tp+i*2)*2)[::-1] for i in range(n)]
        header=struct.unpack_from('<4H',textures,th*2)
        if (attr>>8)&3:faces.append(dict(attr=attr,normal=normal,header=header,vertices=[[*p,*t] for p,t in zip(points,uv)]))
        tp+=n*2;offset=(attr>>12)&31;th+=(offset-32 if offset&16 else offset)*4
        typ=(attr>>8)&3
        if typ in [0,2]:p0,p1=p2,p3
        elif typ==1:p1=p2
        else:p0=p3
    raise ValueError('Missing bounded terminator')

def extract(archive,table_file,output):
    output=Path(output).resolve();media=Path(archive).resolve();table_file=Path(table_file).resolve()
    if output.exists():raise FileExistsError('Use a fresh private output directory')
    if 'research' not in output.parts and 'private' not in output.parts:raise ValueError('Original assets must remain in private/research output')
    rom=program(media);table=table_file.read_bytes()
    if len(table)!=0x40000:raise ValueError('Incomplete observed model table')
    if struct.unpack_from('<3I',rom,0x31600)!=(0x141,0x143,0x14c):raise ValueError('Original scene-model table fingerprint mismatch')
    polygons=region(media,['mpr-18513.16','mpr-18510.20']);textures=region(media,['mpr-18512.25','mpr-18511.24'])
    output.mkdir(parents=True)
    report={'schema':2,'source':'Original vcop2 pickup geometry plus scene-pistol cosmetic; no exact player-held standard identity is asserted','runtime_table_sha256':hashlib.sha256(table).hexdigest(),'weapons':[]}
    fixture=[str(len(NAMES))]
    for weapon,name in NAMES.items():
        reference=0x31604 if weapon==1 else 0x391d4+weapon*16
        model,=struct.unpack_from('<I',rom,reference)
        if model!=MODELS[weapon]:raise ValueError('Original gun model reference fingerprint mismatch')
        mesh=decode(table,polygons,textures,model);mesh.update(weapon=weapon,name=name,equipped_asset=weapon!=1,source_reference=reference)
        (output/(name+'.json')).write_text(json.dumps(mesh,separators=(',',':'))+'\n')
        purpose='scene-pistol cosmetic' if weapon==1 else name+' pickup'
        lines=['# Original Virtua Cop 2 '+purpose+'; private owned media extraction'];n=1
        for face in mesh['faces']:
            lines += ['v '+' '.join(map(str,v[:3])) for v in face['vertices']]
            lines += ['f '+' '.join(str(n+i) for i in range(len(face['vertices'])))];n+=len(face['vertices'])
        (output/(name+'.obj')).write_text('\n'.join(lines)+'\n')
        fixture.append(f"{weapon} {model} {mesh['object']} {len(mesh['faces'])}")
        for face in mesh['faces']:
            values=[len(face['vertices']),face['attr'],*face['header'],*face['normal'],*(v for vertex in face['vertices'] for v in vertex)]
            fixture.append(' '.join(map(str,values)))
        report['weapons'].append({'weapon':weapon,'name':name,'model':model,'object':mesh['object'],'faces':len(mesh['faces']),'equipped_asset':weapon!=1,'source_reference':reference})
    (output/'original-weapons.txt').write_text('\n'.join(fixture)+'\n')
    (output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');return report
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rom',type=Path,required=True);p.add_argument('--table',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();print(json.dumps(extract(a.rom,a.table,a.output),indent=2))
