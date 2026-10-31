"""Render a private contact sheet of owned native model-table entries.

Diagnostic only: a recognizable shape does not establish an equipped-weapon ID.
No game assets are embedded; output must remain in a private research directory.
"""
import argparse,json,math,struct
from pathlib import Path
from extract_weapon_models import decode

def inspect(source,start,end,output):
    from PIL import Image,ImageDraw
    source,output=Path(source).resolve(),Path(output).resolve()
    if output.exists():raise FileExistsError('Use a fresh private output directory')
    if 'research' not in output.parts:raise ValueError('Original model images must remain in private research')
    table,polygons,textures=[(source/(name+'.bin')).read_bytes() for name in ('modeltable','polygons','textures')]
    models=[]
    for model in range(start,min(end,len(table)//16)):
        try:
            mesh=decode(table,polygons,textures,model)
            points=[v[:3] for f in mesh['faces'] for v in f['vertices']]
            if not points:continue
            low=[min(p[i] for p in points) for i in range(3)]
            high=[max(p[i] for p in points) for i in range(3)]
            models.append((mesh,low,high))
        except (ValueError,IndexError,OverflowError,struct.error):pass
    if not models:raise ValueError('No valid native models in requested range')
    columns=6;width,height=240,190
    sheet=Image.new('RGB',(columns*width,math.ceil(len(models)/columns)*height),(20,23,28))
    draw=ImageDraw.Draw(sheet);manifest=[]
    for n,(mesh,low,high) in enumerate(models):
        ox,oy=(n%columns)*width,(n//columns)*height
        center=[(low[i]+high[i])/2 for i in range(3)]
        def project(v):
            x,y,z=[v[i]-center[i] for i in range(3)]
            # View with authored +Y up and all three axes visible.
            a,b=math.radians(25),math.radians(12)
            rx=x*math.cos(a)+z*math.sin(a);rz=-x*math.sin(a)+z*math.cos(a)
            return rx,y*math.cos(b)-rz*math.sin(b),y*math.sin(b)+rz*math.cos(b)
        faces=[([project(v) for v in f['vertices']],f['normal']) for f in mesh['faces']]
        allpoints=[p for face,_ in faces for p in face]
        extent=max(max(p[0] for p in allpoints)-min(p[0] for p in allpoints),
                   (max(p[1] for p in allpoints)-min(p[1] for p in allpoints))*1.5,1e-8)
        scale=210/extent
        for points,normal in sorted(faces,key=lambda f:sum(p[2] for p in f[0])/len(f[0]),reverse=True):
            light=max(.15,min(1,.4+abs(normal[0]*.3+normal[1]*.7+normal[2]*.6)*.6))
            shade=int(90+130*light)
            draw.polygon([(ox+width/2+p[0]*scale,oy+height/2+5-p[1]*scale) for p in points],fill=(shade,shade,shade),outline=(55,66,79))
        draw.text((ox+7,oy+5),f"{mesh['model']:04x}  {len(mesh['faces'])} faces",fill=(235,240,255))
        size=[high[i]-low[i] for i in range(3)]
        draw.text((ox+7,oy+height-15),' x '.join(f'{v:.3g}' for v in size),fill=(180,205,235))
        manifest.append(dict(model=mesh['model'],object=mesh['object'],faces=len(mesh['faces']),low=low,high=high))
    output.mkdir(parents=True)
    sheet.save(output/'contact-sheet.png')
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return dict(models=len(models),output=str(output))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--start',type=lambda v:int(v,0),required=True)
    p.add_argument('--end',type=lambda v:int(v,0),required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();print(json.dumps(inspect(a.source,a.start,a.end,a.output)))
