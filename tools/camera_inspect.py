"""Verify original VC2 camera provenance against locally supplied owned media.

Outputs are private research; no original program bytes are included here.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
from state_disassemble import Decoder, program


def inspect(rom_path, source, output):
    output = Path(output).resolve()
    if 'research' not in output.parts and 'private' not in output.parts:
        raise ValueError('Camera proof must remain in private research output')
    output.mkdir(parents=True, exist_ok=True)
    data = program(rom_path)
    header = Path(__file__).resolve().parents[1]/'native/camera_state.h'
    expected = [(int(a, 16), int(w, 16)) for a, w in re.findall(r'\{(0x[0-9a-f]+),(0x[0-9a-f]+)\}', header.read_text())]
    checks = [{'address': hex(a), 'expected': hex(w), 'actual': hex(struct.unpack_from('<I', data, a)[0]),
               'matches': struct.unpack_from('<I', data, a)[0] == w} for a, w in expected]
    if len(checks) < 20 or not all(item['matches'] for item in checks):
        raise ValueError('Camera fingerprint does not match the original program')
    decoder = Decoder(source, data)
    listings = []
    for start, end in [(0x15f84, 0x15ff0), (0x1be54, 0x1be88), (0x15498, 0x15584),
                       (0x3a7a4, 0x3a854), (0x1c8f4, 0x1c920), (0x1cadc, 0x1cb2c)]:
        listings.extend(decoder.listing(start, end))
        listings.append('')
    (output/'original-camera-listing.txt').write_text('\n'.join(listings))
    (output/'program.bin').write_bytes(data)
    target = 110
    tangent = math.tan(math.radians(target/2))
    report = {'original_program_sha256': hashlib.sha256(data).hexdigest(),
              'fingerprints': checks, 'all_match': True,
              'animated_horizontal_fov_degrees': '0x506034; must not be changed',
              'derived_focal': '0x506044; consumed by GEO and native screen projection',
              'derived_tangent': '0x506140; consumed by distance/culling helper',
              'target_horizontal_fov_degrees': target,
              'target_focal': 248/tangent, 'target_tangent': tangent,
              'original_tgp_trigonometry': 'Approximate lookup; analytical target preserves reciprocal product 248'}
    (output/'camera-proof.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = inspect(args.rom, args.source, args.output)
    print(json.dumps({'all_match': result['all_match'], 'fingerprints': len(result['fingerprints']),
                      'target_focal': result['target_focal'], 'target_tangent': result['target_tangent']}, indent=2))
