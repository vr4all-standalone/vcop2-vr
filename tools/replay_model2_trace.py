"""Replay complete private Model 2 captures through the production GL renderer.

Run with --exe <replay_model2_frames.exe> --captures <scenes> --output <private-dir>.
No copyrighted image or game resource belongs in a source release.
"""
import argparse
import json
from pathlib import Path
import subprocess


def geometry(data):
    rows = [[data[k] for k in ("sequence", "width", "height", "xoffset", "yoffset")]
            + [len(data["polygons"])]]
    for p in data["polygons"]:
        row = [len(p["vertices"])] + [p[k] for k in ("attr", "window", "sort", "luma", "lod", "object")]
        row += [int(p["culled"])] + p["header"] + p["focus"] + p["center"] + p["viewport"]
        rows.append(row + [value for vertex in p["vertices"] for value in vertex])
    return "\n".join(" ".join(str(v) for v in row) for row in rows) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--max-mae", type=float, default=1.0)
    parser.add_argument("--min-exact", type=float, default=0.98)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    import numpy as np
    from PIL import Image
    results = []
    for metadata in sorted(args.captures.glob("*/metadata.json")):
        data = json.loads(metadata.read_text())
        if data["rejected"]:
            raise ValueError(f"Rejected polygons in {metadata}")
        name = metadata.parent.name
        text = args.output / f"{name}.txt"
        text.write_text(geometry(data))
        Path(str(text)+".camera").write_text(f"{data.get('native_fov', 0)} {data.get('native_reference_fov', 0)}\n")
        Path(str(text)+".matrices").write_text("\n".join(" ".join(map(str,p.get('matrix', [0]*12))) for p in data['polygons']))
        Path(str(text)+".culls").write_text("\n".join(str(p.get('cull_reasons', 0)) for p in data['polygons']))
        prefix = args.output / name
        process = subprocess.run([str(args.exe.resolve()), str(text.resolve()), str(metadata.parent.resolve()), str(prefix.resolve())],
                                 capture_output=True, text=True, check=True)
        shape = data["height"], data["width"]
        native = np.fromfile(metadata.parent / "native-argb32.bin", dtype="<u4").reshape(shape)
        reference = np.stack([(native >> 16) & 255, (native >> 8) & 255, native & 255], axis=2).astype(np.uint8)
        neutral = np.asarray(Image.open(str(prefix) + "-neutral.ppm")).astype(np.uint8)
        left = np.asarray(Image.open(str(prefix) + "-left.ppm"))
        right = np.asarray(Image.open(str(prefix) + "-right.ppm"))
        left_wide = np.asarray(Image.open(str(prefix) + "-left-wide.ppm"))
        right_wide = np.asarray(Image.open(str(prefix) + "-right-wide.ppm"))
        error = np.abs(neutral.astype(np.int16) - reference.astype(np.int16))
        mismatch = np.any(error, axis=2)
        foreground = np.fromfile(metadata.parent / "foreground-argb32.bin", dtype="<u4").reshape(shape) != 0
        entry = {"frame": name, "polygons": len(data["polygons"]), "pixels": int(mismatch.size),
                 "exact_pixels": int(np.count_nonzero(~mismatch)), "within_3_pixels": int(np.count_nonzero(np.max(error, axis=2) <= 3)),
                 "rgb_mae": float(error.mean()), "foreground_mismatches": int(np.count_nonzero(mismatch & foreground)),
                 "wider_view_changed_pixels": int(np.count_nonzero(np.any(left_wide != left, axis=2)) + np.count_nonzero(np.any(right_wide != right, axis=2))),
                 "stereo_different_pixels": int(np.count_nonzero(np.any(left != right, axis=2))), "gl": process.stdout.strip()}
        if entry["rgb_mae"] > args.max_mae or entry["exact_pixels"] / entry["pixels"] < args.min_exact:
            raise RuntimeError(f"Neutral replay fidelity gate failed: {entry}")
        if not data["polygons"] and entry["exact_pixels"] != entry["pixels"]:
            raise RuntimeError(f"Native fallback changed pixels: {entry}")
        if entry["foreground_mismatches"]:
            raise RuntimeError(f"Foreground compositing mismatch: {entry}")
        if len(data["polygons"]) and not entry["stereo_different_pixels"]:
            raise RuntimeError(f"Identical stereo output: {entry}")
        Image.fromarray(reference).save(str(prefix) + "-native.png")
        Image.fromarray(neutral).save(str(prefix) + "-neutral.png")
        Image.fromarray(np.minimum(error * 4, 255).astype(np.uint8)).save(str(prefix) + "-difference.png")
        Image.fromarray(np.concatenate([left, right], axis=1)).save(str(prefix) + "-stereo.png")
        Image.fromarray(np.concatenate([left_wide, right_wide], axis=1)).save(str(prefix) + "-stereo-wide.png")
        results.append(entry)
        print(json.dumps(entry), flush=True)
    (args.output / "metrics.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
