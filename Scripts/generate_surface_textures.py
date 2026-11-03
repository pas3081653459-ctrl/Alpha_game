#!/usr/bin/env python3
"""Create deterministic, tileable source textures without third-party packages."""
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'SourceArt/Surfaces'
SIZE = 256


def write_png(path, pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    rows = b''.join(b'\0' + pixels[y * SIZE * 3:(y + 1) * SIZE * 3] for y in range(SIZE))
    path.write_bytes(b'\x89PNG\r\n\x1a\n'
                     + chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE, SIZE, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    heights = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            # Periodic octaves keep the pattern seamless; hashed micrograin is
            # deliberately subtle so it remains a stylized surface at distance.
            u, v = x / SIZE * math.tau, y / SIZE * math.tau
            grain = (((x * 73856093) ^ (y * 19349663) ^ 92821) & 255) / 255 - .5
            height = .5 + .12 * math.sin(u * 3 + .3 * math.cos(v * 2))
            height += .07 * math.cos(v * 7 + math.sin(u * 2)) + .04 * math.sin(u * 17 + v * 13) + .04 * grain
            row.append(max(0., min(1., height)))
        heights.append(row)
    detail, normals = bytearray(), bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            value = round(heights[y][x] * 255)
            detail.extend((value, value, value))
            dx = (heights[y][(x + 1) % SIZE] - heights[y][(x - 1) % SIZE]) * .45
            dy = (heights[(y + 1) % SIZE][x] - heights[(y - 1) % SIZE][x]) * .45
            length = math.sqrt(dx * dx + dy * dy + 1)
            normals.extend(round((component / length * .5 + .5) * 255) for component in (-dx, -dy, 1.))
    files = [('T_ForgeDetail.png', detail), ('T_ForgeNormal_N.png', normals)]
    for name, pixels in files:
        write_png(OUTPUT / name, pixels)
    manifest = {
        'generator': 'AlphaGame.generate_surface_textures.v1', 'size': SIZE,
        'provenance': 'Original mathematical textures; no downloaded or licensed source imagery.',
        'files': {name: hashlib.sha256((OUTPUT / name).read_bytes()).hexdigest() for name, _ in files},
    }
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Generated {len(files)} deterministic {SIZE}x{SIZE} source textures in {OUTPUT}')


if __name__ == '__main__':
    main()
