"""Reconstruct pinned patched files and update only recognized compiler states."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def synchronize(engine, patches, commit):
    engine = Path(engine).resolve()
    patch_files = sorted(Path(patches).glob('*.patch'))
    paths = sorted({p for patch in patch_files for p in re.findall(r'^\+\+\+ b/(.+)$', patch.read_text(), re.M)})
    if any(Path(p).is_absolute() or '..' in Path(p).parts for p in paths):
        raise ValueError('Unsafe patch path')
    with tempfile.TemporaryDirectory(prefix='vcop2-patches-', dir=engine.parent) as temporary:
        root = Path(temporary)
        known = {p: set() for p in paths}
        for path in paths:
            original = subprocess.check_output(['git', '-C', str(engine), 'show', f'{commit}:{path}']).replace(b'\r\n', b'\n')
            target = root/path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(original)
            known[path].add(original)
        for patch in patch_files:
            subprocess.run(['git', '-c', 'core.autocrlf=false', 'apply', '--whitespace=nowarn', '-'], input=patch.read_bytes().replace(b'\r\n', b'\n'), cwd=root, check=True)
            for path in paths:
                known[path].add((root/path).read_bytes().replace(b'\r\n', b'\n'))
        changes = []
        for path in paths:
            raw = (engine/path).read_bytes()
            actual = raw.replace(b'\r\n', b'\n')
            if actual not in known[path]:
                import difflib
                closest = min(known[path], key=lambda candidate: abs(len(candidate)-len(actual)))
                difference = ''.join(difflib.unified_diff(closest.decode().splitlines(True), actual.decode().splitlines(True)))
                raise ValueError('Compiler file has unrecognized edits: '+path+'\n'+difference[:3000])
            expected = (root/path).read_bytes().replace(b'\r\n', b'\n')
            if actual != expected:
                changes.append((engine/path, expected.replace(b'\n', b'\r\n') if b'\r\n' in raw else expected))
        for target, data in changes:
            target.write_bytes(data)
        print(f'Verified {len(paths)} pinned engine files; updated {len(changes)}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--patches', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    synchronize(args.engine, args.patches, args.commit)
