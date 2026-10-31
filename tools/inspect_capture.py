"""Render private native capture diagnostics (requires Pillow and NumPy)."""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from verify_captures import validate_capture


def read_capture(metadata):
    metadata = Path(metadata).resolve()
    if metadata.stat().st_size > 32 * 1024 * 1024:
        raise ValueError('Oversized scene metadata')
    data = json.loads(metadata.read_text())
    if data['schema'] != 1 or not (1 <= data['width'] <= 2048 and 1 <= data['height'] <= 2048):
        raise ValueError('Unsupported scene dimensions or schema')
    if data['rejected'] or len(data['polygons']) > 32768:
        raise ValueError('Incomplete or oversized scene capture')
    return metadata.parent, data


def native_image(root, data):
    raw = (root / 'native-argb32.bin').read_bytes()
    width, height = data['width'], data['height']
    if len(raw) != width * height * 4:
        raise ValueError('Incomplete native raster')
    argb = np.frombuffer(raw, dtype='<u4').reshape(height, width)
    rgb = np.stack(((argb >> 16) & 255, (argb >> 8) & 255, argb & 255), axis=-1).astype('uint8')
    return Image.fromarray(rgb)


def native_point(vertex, polygon, frame):
    x, y, z, _, _ = vertex
    if not all(math.isfinite(n) for n in (x, y, z)) or z <= 1e-6:
        return None
    # Exact model2_state::model2_3d_project; X/Y already include native focus.
    center_x, center_y = polygon['center']
    point = frame['xoffset'] + center_x + x / z, 384 - center_y + frame['yoffset'] - y / z
    return point if all(math.isfinite(n) and abs(n) < 100000 for n in point) else None


def inspect(metadata, output):
    validate_capture(Path(metadata).resolve().parent)
    root, data = read_capture(metadata)
    output = Path(output).resolve()
    if output.is_relative_to(root) or root.is_relative_to(output):
        raise ValueError('Keep diagnostic output separate from captured evidence')
    output.mkdir(parents=True, exist_ok=False)
    image = native_image(root, data)
    image.save(output / 'native.png')
    lines = Image.new('RGBA', image.size)
    draw = ImageDraw.Draw(lines)
    count = 0
    for polygon in data['polygons']:
        if polygon['culled']:
            continue
        points = [native_point(v, polygon, data) for v in polygon['vertices']]
        if any(p is None for p in points):
            continue
        # Raw polygons are intentionally captured before native frustum clipping.
        # Pillow clips diagnostic lines to the image; this is not a scene renderer.
        draw.line(points + [points[0]], fill=(0, 255, 150, 90), width=1)
        count += 1
    Image.alpha_composite(image.convert('RGBA'), lines).convert('RGB').save(output / 'native-wireframe.png')
    report = {'sequence': data['sequence'], 'time': data['time'], 'polygons': len(data['polygons']),
              'outlined_polygons': count, 'native_objects': len({p['object'] for p in data['polygons']})}
    (output / 'inspection.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('metadata', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(inspect(args.metadata, args.output), indent=2))
