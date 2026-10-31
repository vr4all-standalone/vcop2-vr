"""Create and validate local software-only Virtua Cop 2 VR deliveries."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_ANDROID_LIBS = {'libmain.so', 'libSDL2.so', 'libc++_shared.so', 'libvcop2_xr.so', 'libopenxr_loader.so'}
REQUIRED_WINDOWS = {'vcop2.exe', 'vcop2_xr.dll', 'openxr_loader.dll'}
VERSION = '0.2.14'


def check_names(names):
    """Reject traversal and media/state/keys before writing a public artifact."""
    rom_names = {x['name'].casefold() for x in json.loads((ROOT/'game-set.json').read_text())['files']}
    seen = set()
    for name in names:
        normalized = name.replace('\\', '/')
        path = PurePosixPath(normalized)
        parts = {part.casefold() for part in path.parts}
        key = normalized.casefold()
        if key in seen:
            raise ValueError(f'Duplicate package member: {name}')
        seen.add(key)
        private = {'private', 'research', 'userdata', 'nvram', 'cfg', 'state', 'snap', '.git'}
        if (not normalized or path.is_absolute() or ':' in normalized or '..' in path.parts
                or parts & private or path.name.casefold() in rom_names
                or path.name.casefold() in {'settings.ini', 'vcop2.zip', 'segabill.zip', 'custom.glb'}
                or path.suffix.casefold() in {'.zip', '.chd', '.nv', '.dif', '.jks', '.keystore', '.pem', '.key', '.log'}
                or (path.name.casefold().startswith('original') and path.suffix.casefold() in {'.glb', '.gltf', '.png', '.bin'})):
            raise ValueError(f'Private/game data cannot be packaged: {name}')


def _elf_arm64(header, name):
    if len(header) < 64 or header[:7] != b'\x7fELF\x02\x01\x01' or struct.unpack_from('<H', header, 18)[0] != 183:
        raise ValueError(f'Wrong native ABI: {name}; ARM64 ELF is required')


def check_apk(path):
    """Structural/media check; callers must separately verify the APK signature."""
    with zipfile.ZipFile(path) as archive:
        check_names(archive.namelist())
        names = set(archive.namelist())
        required = {'lib/arm64-v8a/'+name for name in REQUIRED_ANDROID_LIBS}
        if not required <= names:
            raise ValueError(f'Missing ARM64 libraries: {sorted(required-names)}')
        if 'AndroidManifest.xml' not in names:
            raise ValueError('APK has no Android manifest')
        for name in required:
            with archive.open(name) as stream:
                _elf_arm64(stream.read(64), name)
        for name in names:
            if name.startswith('lib/') and not name.startswith('lib/arm64-v8a/'):
                raise ValueError(f'Unexpected native ABI: {name}')
        font = archive.getinfo('assets/fonts/Roboto-Bold.ttf')
        with archive.open(font) as stream:
            if not 20000 <= font.file_size <= 5*1024*1024 or stream.read(4) != b'\0\1\0\0':
                raise ValueError('Menu font is not a complete TrueType font')
        damaged = archive.testzip()
        if damaged:
            raise ValueError(f'Corrupt APK member: {damaged}')
    return {'native_abi': 'arm64-v8a', 'libraries': sorted(REQUIRED_ANDROID_LIBS), 'game_data_included': False, 'signature_verified': False}


def verify_signature(path, apksigner):
    result = subprocess.run([str(apksigner), 'verify', '--verbose', '--print-certs', str(path)], check=True, capture_output=True, text=True)
    return {'signature_verified': True, 'signature_report': result.stdout.strip()}


def _pe_amd64(stream, name):
    header = stream.read(64)
    if len(header) != 64 or header[:2] != b'MZ':
        raise ValueError(f'Not a Windows executable: {name}')
    offset = struct.unpack_from('<I', header, 60)[0]
    if offset < 64 or offset > 1024*1024:
        raise ValueError(f'Invalid PE header: {name}')
    stream.read(offset-64)
    signature = stream.read(6)
    if len(signature) != 6 or signature[:4] != b'PE\0\0' or struct.unpack_from('<H', signature, 4)[0] != 0x8664:
        raise ValueError(f'Wrong Windows ABI: {name}; AMD64 is required')


def check_windows(path):
    with zipfile.ZipFile(path) as archive:
        check_names(archive.namelist())
        names = {PurePosixPath(name).name.casefold() for name in archive.namelist()}
        if not REQUIRED_WINDOWS <= names:
            raise ValueError(f'Missing Windows components: {sorted(REQUIRED_WINDOWS-names)}')
        for name in archive.namelist():
            if PurePosixPath(name).suffix.casefold() in {'.exe', '.dll'}:
                with archive.open(name) as stream: _pe_amd64(stream, name)
        damaged = archive.testzip()
        if damaged: raise ValueError(f'Corrupt Windows package: {damaged}')
    return {'native_abi': 'x86_64', 'game_data_included': False}


def windows(engine, host, loader, mingw, readobj, output):
    output.mkdir(parents=True, exist_ok=True)
    (ROOT/'build').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='vcop2-package-', dir=ROOT/'build') as temporary:
        folder = Path(temporary)/'vcop2-vr-pcvr'; folder.mkdir()
        queue = [engine, host, loader]; seen = set()
        for source in queue:
            if source.name.casefold() in seen: continue
            seen.add(source.name.casefold())
            with source.open('rb') as stream: _pe_amd64(stream, source.name)
            target_name = 'openxr_loader.dll' if source == loader else source.name
            shutil.copy2(source, folder/target_name)
            imports = subprocess.run([str(readobj), '--coff-imports', str(source)], check=True, text=True, capture_output=True).stdout
            for name in re.findall(r'^\s*Name: (.+\.dll)\s*$', imports, re.MULTILINE|re.IGNORECASE):
                dependency = mingw/name
                if dependency.is_file(): queue.append(dependency)
        for source in ['tools/launch-pcvr.ps1', 'tools/run-pcvr.cmd', 'CREDITS.md', 'LICENSE', 'engine-lock.json', 'game-set.json', 'README.md', 'docs/validation.md', 'docs/hud-size.md', f'docs/release-notes-{VERSION}.md']:
            target = folder/(Path(source).name if source.startswith('tools/') else source)
            target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(ROOT/source, target)
        for directory in ['assets', 'licenses']:
            for source in (ROOT/directory).rglob('*'):
                if source.is_file():
                    relative = source.relative_to(ROOT); check_names([relative.as_posix()])
                    target = folder/relative; target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(source, target)
        (folder/'README.txt').write_text('''Virtua Cop 2 VR — local development build

Enable your PC OpenXR runtime and connect the headset. Run run-pcvr.cmd.
Place owned original arcade files in roms/vcop2.zip and roms/segabill.zip,
or supply a complete vcop2.zip containing all 31 required ROMs.
To use another ROM folder: launch-pcvr.ps1 -RomDirectory D:\\MyRoms
Keep ROM archives packed. NVRAM, settings and saved state use userdata/.
No game data is included. See CREDITS.md, licenses/ and docs/validation.md
for implementation and physical-headset validation status.
''', encoding='utf-8')
        (folder/'build-info.json').write_text(json.dumps({'engine': json.loads((ROOT/'engine-lock.json').read_text()), 'development_build': True, 'game_data_included': False}, indent=2), encoding='utf-8')
        files = sorted(file for file in folder.rglob('*') if file.is_file())
        check_names([file.relative_to(folder).as_posix() for file in files])
        archive = output/f'vcop2-vr-pcvr-{VERSION}.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as zipout:
            for file in files: zipout.write(file, 'vcop2-vr-pcvr/'+file.relative_to(folder).as_posix())
    check_windows(archive)
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check-apk', type=Path)
    parser.add_argument('--check-windows', type=Path)
    parser.add_argument('--apksigner', type=Path, default=Path('E:/AndroidSdk/build-tools/36.0.0/apksigner.bat'))
    parser.add_argument('--apk', type=Path, default=ROOT/'build/android/app/outputs/apk/release/app-release.apk')
    parser.add_argument('--windows-only', action='store_true', help='Package verified PCVR binaries without requiring a Quest APK')
    parser.add_argument('--engine', type=Path); parser.add_argument('--host', type=Path)
    parser.add_argument('--loader', type=Path); parser.add_argument('--mingw-bin', type=Path); parser.add_argument('--readobj', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'build/delivery')
    args = parser.parse_args()
    if args.check_apk:
        report = check_apk(args.check_apk); report.update(verify_signature(args.check_apk, args.apksigner))
        print(json.dumps(report, indent=2)); return
    if args.check_windows: print(json.dumps(check_windows(args.check_windows), indent=2)); return
    args.output.mkdir(parents=True, exist_ok=True)
    files = []; report = {}
    if not args.windows_only:
        quest = check_apk(args.apk); quest.update(verify_signature(args.apk, args.apksigner))
        apk = args.output/f'vcop2-vr-quest-{VERSION}.apk'; shutil.copy2(args.apk, apk)
        files.append(apk); report['quest'] = quest
    if args.windows_only or any([args.engine, args.host, args.loader, args.mingw_bin, args.readobj]):
        if not all([args.engine, args.host, args.loader, args.mingw_bin, args.readobj]): parser.error('PC packaging requires --engine, --host, --loader, --mingw-bin and --readobj')
        archive = windows(args.engine, args.host, args.loader, args.mingw_bin, args.readobj, args.output)
        files.append(archive); report['windows'] = check_windows(archive)
    with (args.output/'SHA256SUMS.txt').open('w', encoding='utf-8') as out:
        for file in files:
            with file.open('rb') as incoming: digest = hashlib.file_digest(incoming, 'sha256').hexdigest()
            out.write(f'{digest}  {file.name}\n')
    (args.output/'artifact-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'files': list(map(str, files)), 'validation': report}, indent=2))


if __name__ == '__main__': main()
