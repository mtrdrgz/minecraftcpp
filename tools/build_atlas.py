"""
build_atlas.py — Extract block textures from client.jar and stitch a texture atlas
Outputs: block_atlas.png + block_atlas.json
"""
import zipfile
import json
import math
import struct
import sys
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JAR = Path(os.environ.get("MCPP_CLIENT_JAR", ROOT / "26.1.2" / "client.jar"))
OUT_DIR = Path(os.environ.get("MCPP_ASSETS_OUT", ROOT / "mcpp" / "src" / "assets"))

# Try to use Pillow for image processing
try:
    from PIL import Image
    import io
    HAS_PILLOW = True
except ImportError:
    HAS_PILLOW = False
    print("Pillow not found, installing...")
    os.system("pip install Pillow -q")
    from PIL import Image
    import io
    HAS_PILLOW = True

def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Reading {JAR}...")
    textures = {}  # name -> PIL.Image

    with zipfile.ZipFile(JAR) as zf:
        names = zf.namelist()
        block_pngs = [n for n in names
                      if n.startswith("assets/minecraft/textures/block/")
                      and n.endswith(".png")
                      and not n.endswith(".mcmeta")]
        print(f"Found {len(block_pngs)} block textures")

        for path in block_pngs:
            name = Path(path).stem  # e.g. "stone"
            data = zf.read(path)
            try:
                img = Image.open(io.BytesIO(data)).convert("RGBA")
                # If animated (height > width), use only first frame
                if img.height > img.width and img.height % img.width == 0:
                    img = img.crop((0, 0, img.width, img.width))
                # Ensure 16x16
                if img.size != (16, 16):
                    img = img.resize((16, 16), Image.NEAREST)
                textures[name] = img
            except Exception as e:
                print(f"  Skip {name}: {e}")

    print(f"Loaded {len(textures)} usable textures")

    # Sort for reproducibility
    names_sorted = sorted(textures.keys())
    n = len(names_sorted)

    # Calculate atlas size (power-of-2 square)
    tiles_per_row = 1
    while tiles_per_row * tiles_per_row < n:
        tiles_per_row *= 2
    atlas_size = tiles_per_row * 16
    print(f"Atlas: {atlas_size}x{atlas_size} ({tiles_per_row}x{tiles_per_row} tiles)")

    atlas = Image.new("RGBA", (atlas_size, atlas_size), (0, 0, 0, 0))
    uv_map = {}  # name -> [u0, v0, u1, v1] in 0..1

    for i, name in enumerate(names_sorted):
        col = i % tiles_per_row
        row = i // tiles_per_row
        px = col * 16
        py = row * 16
        atlas.paste(textures[name], (px, py))
        uv_map[name] = [
            px / atlas_size,
            py / atlas_size,
            (px + 16) / atlas_size,
            (py + 16) / atlas_size,
        ]

    # Add fallback texture (magenta/black checkerboard for missing blocks)
    missing_img = Image.new("RGBA", (16, 16))
    for y in range(16):
        for x in range(16):
            color = (200, 0, 200, 255) if (x + y) % 2 == 0 else (0, 0, 0, 255)
            missing_img.putpixel((x, y), color)
    uv_map["__missing__"] = uv_map.get("missing_texture",
                                        uv_map.get("stone", [0, 0, 1/tiles_per_row, 1/tiles_per_row]))

    # Save atlas PNG
    atlas_png = OUT_DIR / "block_atlas.png"
    atlas.save(atlas_png)
    print(f"Saved atlas: {atlas_png} ({atlas_png.stat().st_size // 1024} KB)")

    # Save UV map JSON
    atlas_json = OUT_DIR / "block_atlas.json"
    with open(atlas_json, "w") as f:
        json.dump({"tiles_per_row": tiles_per_row,
                   "atlas_size": atlas_size,
                   "textures": uv_map}, f, separators=(",", ":"))
    print(f"Saved UV map: {atlas_json} ({len(uv_map)} textures)")

    # Print some important mappings
    for name in ["stone", "grass_block_top", "grass_block_side", "dirt",
                 "oak_planks", "sand", "gravel", "oak_log", "oak_leaves"]:
        if name in uv_map:
            uv = uv_map[name]
            print(f"  {name}: [{uv[0]:.4f}, {uv[1]:.4f}, {uv[2]:.4f}, {uv[3]:.4f}]")

if __name__ == "__main__":
    main()
