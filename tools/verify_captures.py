"""Check completed native captures, including exact foreground compositing."""
import argparse
import json
from pathlib import Path
import re
import struct


def validate_capture(directory):
    directory = Path(directory).resolve()
    metadata = directory / 'metadata.json'
    if metadata.stat().st_size > 32 * 1024 * 1024:
        raise ValueError('Oversized scene metadata')
    data = json.loads(metadata.read_text())
    width, height = data['width'], data['height']
    if data['schema'] != 1 or not (1 <= width <= 2048 and 1 <= height <= 2048):
        raise ValueError('Invalid native dimensions or capture schema')
    if data['rejected'] or len(data['polygons']) > 32768:
        raise ValueError('Incomplete scene geometry')
    for blob in data['blobs']:
        if not re.fullmatch(r'[a-z0-9_-]{1,64}', blob['name']) or not 1 <= blob['size'] <= 4 * 1024 * 1024:
            raise ValueError('Invalid native blob name or bounds')
        if (directory / (blob['name'] + '.bin')).stat().st_size != blob['size']:
            raise ValueError('Incomplete native state blob')
    rasters = {}
    for name in ['native', 'background', 'foreground']:
        raw = (directory / (name + '-argb32.bin')).read_bytes()
        if len(raw) != width * height * 4:
            raise ValueError('Incomplete ' + name + ' raster')
        rasters[name] = raw
    # The original driver copies every nonzero foreground RGB pen onto the
    # final native image. These pixels must match exactly, regardless of scene.
    mismatches = sum(1 for (fg,), (native,) in zip(struct.iter_unpack('<I', rasters['foreground']),
                                                 struct.iter_unpack('<I', rasters['native'])) if fg and fg != native)
    if mismatches:
        raise ValueError(f'Foreground differs from final native composition at {mismatches} pixels')
    return {'sequence': data['sequence'], 'polygons': len(data['polygons']), 'foreground_mismatches': 0}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture_root', type=Path)
    args = parser.parse_args()
    files = sorted(args.capture_root.glob('*/metadata.json'))
    if not files:
        parser.error('No completed native captures')
    print(json.dumps([validate_capture(file.parent) for file in files], indent=2))
