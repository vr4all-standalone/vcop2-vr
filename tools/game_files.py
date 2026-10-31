"""Verify original Model 2 ROM payloads and stage a separate canonical ZIP."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import zipfile
import zlib

MAX_MEMBERS=4096

def _requirements(expected):
    if expected is None:
        expected=json.loads((Path(__file__).resolve().parents[1]/'game-set.json').read_text(encoding='utf-8-sig'))['files']
    if not expected or len({x['name'].casefold() for x in expected})!=len(expected):
        raise ValueError('ROM requirements must be nonempty and unique')
    for item in expected:
        if item['size']<=0 or item['size']>64*1024*1024:raise ValueError('Invalid expected ROM size')
    return expected

def _members(archive):
    entries=archive.infolist()
    if len(entries)>MAX_MEMBERS:raise ValueError('Too many ZIP members')
    seen=set()
    for entry in entries:
        name=entry.filename
        if not name or name in {'.','..'} or '/' in name or '\\' in name or ':' in name:
            raise ValueError('Unsafe ROM member: '+name)
        if name.casefold() in seen:raise ValueError('Duplicate ROM member: '+name)
        if entry.flag_bits&1:raise ValueError('Encrypted ROM member: '+name)
        seen.add(name.casefold())
    return entries

def _hash(archive,entry,expected_size):
    sha=hashlib.sha1();crc=0;size=0
    with archive.open(entry) as stream:
        while block:=stream.read(1024*1024):
            size+=len(block)
            if size>expected_size:raise ValueError('Oversized ROM payload: '+entry.filename)
            sha.update(block);crc=zlib.crc32(block,crc)
    return size,f'{crc&0xffffffff:08x}',sha.hexdigest()

def validate_archives(paths,expected=None):
    expected=_requirements(expected)
    found={};errors=[]
    for path in paths:
        path=Path(path).resolve()
        try:
            with zipfile.ZipFile(path) as archive:
                entries=_members(archive)
                for wanted in expected:
                    if wanted['name'] in found:continue
                    candidates=[e for e in entries if e.file_size==wanted['size'] and f'{e.CRC:08x}'==wanted['crc32'].lower()]
                    for entry in candidates:
                        size,crc,sha=_hash(archive,entry,wanted['size'])
                        if size==wanted['size'] and crc==wanted['crc32'].lower() and sha==wanted['sha1'].lower():
                            found[wanted['name']]={'name':wanted['name'],'archive':str(path),'member':entry.filename,'size':size,'crc32':crc,'sha1':sha}
                            break
        except (OSError,ValueError,zipfile.BadZipFile,RuntimeError,NotImplementedError,zlib.error) as error:
            errors.append(f'{path.name}: {error}')
    missing=[x['name'] for x in expected if x['name'] not in found]
    return {'ready':not errors and not missing,'set':'vcop2','verified':len(found),'required':len(expected),'missing':missing,'errors':errors,'checks':list(found.values())}

def stage_verified(paths,output,expected=None):
    expected=_requirements(expected);paths=[Path(x).resolve() for x in paths];output=Path(output).resolve()
    for source in paths:
        if output.is_relative_to(source.parent) or source.is_relative_to(output.parent):
            raise ValueError('Keep original media and writable staging directories separate')
    if output.exists():raise FileExistsError(output)
    report=validate_archives(paths,expected)
    if not report['ready']:raise ValueError('Original ROM verification failed: '+json.dumps({'missing':report['missing'],'errors':report['errors']}))
    output.parent.mkdir(parents=True,exist_ok=True)
    descriptor,temporary=tempfile.mkstemp(prefix='.vcop2-',suffix='.zip',dir=output.parent)
    os.close(descriptor)
    try:
        checks={x['name']:x for x in report['checks']}
        with zipfile.ZipFile(temporary,'w',zipfile.ZIP_DEFLATED) as target:
            for wanted in expected:
                checked=checks[wanted['name']]
                with zipfile.ZipFile(checked['archive']) as source:
                    member=source.getinfo(checked['member'])
                    if member.file_size!=wanted['size']:raise ValueError('Original media changed while staging')
                    with source.open(member) as incoming,target.open(wanted['name'],'w') as outgoing:
                        size=0
                        while block:=incoming.read(1024*1024):
                            size+=len(block)
                            if size>wanted['size']:raise ValueError('Oversized changed ROM')
                            outgoing.write(block)
        staged=validate_archives([temporary],expected)
        if not staged['ready']:raise ValueError('Staged data verification failed')
        # Atomic no-overwrite publication; temp and output share the same filesystem.
        os.link(temporary,output)
        return report
    finally:
        Path(temporary).unlink(missing_ok=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives',nargs='+',type=Path)
    parser.add_argument('--stage',type=Path)
    args=parser.parse_args()
    report=stage_verified(args.archives,args.stage) if args.stage else validate_archives(args.archives)
    print(json.dumps(report,indent=2))
    return 0 if report['ready'] else 1

if __name__=='__main__':raise SystemExit(main())
