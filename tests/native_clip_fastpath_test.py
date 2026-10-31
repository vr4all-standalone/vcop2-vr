"""Compare the production helper with the actual pinned MAME clipper, privately."""
import argparse,json,os,re,subprocess,tempfile
from pathlib import Path

def function(source,signature):
    start=source.index(signature);brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[start:end]+'\n'

def run(source,cxx,captures):
    source,cxx=Path(source).resolve(),Path(cxx).resolve()
    text=source.read_text(encoding='utf8')
    reference=function(text,'static inline float dot_product(')+function(text,'static int32_t clip_polygon(')
    fixture=[];count=0
    for capture in captures:
        files=sorted(Path(capture).rglob('metadata.json'))
        for file in files:
            for polygon in json.loads(file.read_text())['polygons']:
                vertices=polygon['vertices']
                if len(vertices) not in (3,4):continue
                fixture.append(' '.join(map(str,[len(vertices),*polygon['center'],*polygon['viewport'],*(v for point in vertices for v in point)])))
                count+=1
    data=str(count)+'\n'+'\n'.join(fixture)+'\n'
    env=os.environ.copy();env['PATH']=str(cxx.parent)+os.pathsep+env.get('PATH','')
    # Keep compiler temporaries within this writable workspace. The randomly
    # created directory is never a caller-supplied path or a cache junction.
    with tempfile.TemporaryDirectory(prefix='.native-clip-proof-',dir=Path(__file__).resolve().parent) as directory:
        root=Path(directory);(root/'native_clip_reference.h').write_text(reference,encoding='utf8')
        exe=root/('clip-proof.exe' if os.name=='nt' else 'clip-proof')
        subprocess.run([str(cxx),'-std=c++20','-O3','-Wall','-Wextra','-Werror','-I',str(root),
                        '-DVCOP2_CLIP_REFERENCE_HEADER="native_clip_reference.h"',str(Path(__file__).with_suffix('.cpp')),'-o',str(exe)],env=env,check=True)
        subprocess.run([str(exe)],input=data,text=True,env=env,check=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--cxx',type=Path,required=True)
    p.add_argument('--capture',type=Path,action='append',default=[])
    a=p.parse_args();run(a.source,a.cxx,a.capture)
