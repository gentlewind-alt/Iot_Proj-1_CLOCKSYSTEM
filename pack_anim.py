#!/usr/bin/env python3
import os, re, json, struct, argparse
from pathlib import Path

MAGIC = b"BINPACK\x00"

def numeric_key(name: str):
    # Extract last integer in the name; fallback to name lower()
    m = re.findall(r'(\d+)', name)
    if m:
        return (int(m[-1]), name.lower())
    return (float('inf'), name.lower())

def collect_frame_files(folder: Path):
    frames = [p for p in folder.iterdir() if p.is_file() and p.suffix.lower()=='.bin']
    frames.sort(key=lambda p: numeric_key(p.name))
    return frames

def pack_folder(folder: Path, out_dir: Path, width: int, height: int):
    frames = collect_frame_files(folder)
    if not frames:
        return None

    frame_size = (width * height) // 8
    # Validate frame sizes
    bad = [f for f in frames if f.stat().st_size != frame_size]
    if bad:
        raise RuntimeError(f"Folder '{folder}': {len(bad)} files are not {frame_size} bytes (e.g. {bad[0].name})")

    rel = folder.name  # keep only last segment as "animation name"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{rel}.pbin"

    with out_path.open('wb') as w:
        # Header: MAGIC, width(u16), height(u16), frame_count(u32)
        header = MAGIC + struct.pack('<HHI', width, height, len(frames))
        w.write(header)
        # Concatenate frames
        for f in frames:
            w.write(f.read_bytes())

    return {
        "name": rel,
        "file": str(out_path),
        "frames": len(frames),
        "width": width,
        "height": height,
        "bytes_per_frame": frame_size
    }

def walk_and_pack(root: Path, out: Path, width: int, height: int):
    manifest = {"width": width, "height": height, "animations": []}
    for child in root.iterdir():
        if child.is_dir():
            info = pack_folder(child, out, width, height)
            if info:
                manifest["animations"].append(info)
    # Sort animations by name
    manifest["animations"].sort(key=lambda x: x["name"].lower())
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2))
    return manifest

def main():
    ap = argparse.ArgumentParser(description="Pack 128x64 1bpp frame .bin files into compact .pbin per folder.")
    ap.add_argument('--root', required=True, help='Root folder containing animation subfolders (each with .bin frames).')
    ap.add_argument('--out', default='packs', help='Output folder for packed animations and manifest.json')
    ap.add_argument('--width', type=int, default=128)
    ap.add_argument('--height', type=int, default=64)
    args = ap.parse_args()

    root = Path(args.root)
    out = Path(args.out)

    if not root.exists():
        raise SystemExit(f"Root folder not found: {root}")

    manifest = walk_and_pack(root, out, args.width, args.height)
    print(f"Packed {len(manifest['animations'])} animations into: {out}")
    for a in manifest['animations']:
        print(f" - {a['name']}: {a['frames']} frames -> {a['file']}")

if __name__ == '__main__':
    main()
