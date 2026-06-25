"""Voxel renderer for Minecraft structures.

Bug-fix history:
  Previous version: used a 3D numpy `occupied` array and called ax.voxels().
  Problem: matplotlib's voxels() renders only the *faces* of the occupied
  volume. When two occupied blocks are adjacent (touching faces), matplotlib
  merges them into a single visual unit and the shared face is not drawn.
  For Minecraft structures like fossils (where blocks ARE meant to be
  visible individually even when touching), this caused "adjacent blocks
  disappear" — the user correctly spotted it.

  Fix: render each block as its OWN voxels() call with a tiny gap
  between adjacent blocks. This guarantees every block is visible as a
  distinct cube. Performance is acceptable for ~5000 blocks.

  Alternative fix (implemented below): use Poly3DCollection with one
  6-face cube per block. Same visual result, better control over edge
  drawing. We use this approach.
"""

from __future__ import annotations

from typing import Iterable, Optional

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.font_manager as fm
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

from .blocks import Block, get_color


# Font setup (CJK-safe)
try:
    fm.fontManager.addfont("/usr/share/fonts/truetype/chinese/NotoSansSC-Regular.ttf")
    fm.fontManager.addfont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")
    plt.rcParams["font.sans-serif"] = ["Noto Sans SC", "DejaVu Sans"]
except Exception:
    pass
plt.rcParams["axes.unicode_minus"] = False


# Face direction vectors for a cube
# Each face = 4 vertices (corners), listed in CCW order when viewed from outside
_CUBE_FACES = [
    # -X face (left)
    [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)],
    # +X face (right)
    [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)],
    # -Y face (bottom)
    [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)],
    # +Y face (top)
    [(0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)],
    # -Z face (back)
    [(0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)],
    # +Z face (front)
    [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)],
]


def _build_cube_faces(x: int, y: int, z: int):
    """Return the 6 faces of a unit cube at integer position (x,y,z).
    Each face is a list of 4 (x,y,z) tuples in world coordinates."""
    return [
        [(x + dx, y + dy, z + dz) for dx, dy, dz in face]
        for face in _CUBE_FACES
    ]


def render_blocks(
    blocks: Iterable[Block],
    title: str,
    output_path: str,
    *,
    elev: float = 22.0,
    azim: float = -55.0,
    figsize: tuple[float, float] = (12.0, 9.0),
    dpi: int = 110,
    subtitle: Optional[str] = None,
    bg_color: str = "#0a0a0a",
    block_gap: float = 0.05,
) -> str:
    """Render a list of blocks as a 3D voxel scene.

    Parameters
    ----------
    blocks : iterable of Block
    title : str
    output_path : str
    elev, azim : float
        Camera angles in degrees.
    block_gap : float
        Gap between adjacent blocks, in [0, 0.5). 0 = blocks touch exactly
        (may cause "adjacent faces disappear" visual bug). 0.05 = small
        visible gap so every block reads as a distinct cube.
    """
    blocks = list(blocks)
    if not blocks:
        raise ValueError("Cannot render empty block list.")

    if not (0.0 <= block_gap < 0.5):
        raise ValueError(f"block_gap must be in [0, 0.5), got {block_gap}")

    # Compute bounds
    xs = [b.x for b in blocks]
    ys = [b.y for b in blocks]
    zs = [b.z for b in blocks]
    x_min, x_max = min(xs), max(xs)
    y_min, y_max = min(ys), max(ys)
    z_min, z_max = min(zs), max(zs)
    nx = x_max - x_min + 1
    ny = y_max - y_min + 1
    nz = z_max - z_min + 1

    fig = plt.figure(figsize=figsize, constrained_layout=True)
    ax = fig.add_subplot(111, projection="3d")

    # Group blocks by block_type to batch the Poly3DCollection calls
    # (each block_type gets one collection with N cubes).
    by_type: dict[str, list[tuple[int, int, int]]] = {}
    for b in blocks:
        by_type.setdefault(b.block_type, []).append((b.x, b.y, b.z))

    gap = block_gap
    half = (1.0 - gap) / 2.0

    for block_type, positions in by_type.items():
        base_color = get_color(block_type)
        # Build all faces for all cubes of this type
        all_faces = []
        face_colors = []
        for x, y, z in positions:
            # For each face, scale by (1-gap) around the cube center
            cx, cy, cz = x + 0.5, y + 0.5, z + 0.5
            for face in _CUBE_FACES:
                # Each vertex: (dx, dy, dz) in {0, 1}^3
                # Center it, scale, then translate back
                scaled_face = [
                    (cx + (dx - 0.5) * (1.0 - gap),
                     cy + (dy - 0.5) * (1.0 - gap),
                     cz + (dz - 0.5) * (1.0 - gap))
                    for dx, dy, dz in face
                ]
                all_faces.append(scaled_face)
                face_colors.append(base_color)

        collection = Poly3DCollection(
            all_faces,
            facecolors=face_colors,
            edgecolors=(0.08, 0.08, 0.08, 0.4),
            linewidths=0.2,
        )
        ax.add_collection3d(collection)

    # Set limits and aspect
    pad = 0.5
    ax.set_xlim(x_min - pad, x_max + 1 + pad)
    ax.set_ylim(z_min - pad, z_max + 1 + pad)
    ax.set_zlim(y_min - pad, y_max + 1 + pad)
    ax.set_box_aspect((nx, nz, ny))

    ax.view_init(elev=elev, azim=azim)
    ax.set_axis_off()

    # Title
    full_title = title if not subtitle else f"{title}\n{subtitle}"
    fig.suptitle(full_title, fontsize=14, fontweight="bold", y=0.98)
    info = f"Blocks: {len(blocks):,}   |   Bounds: x[{x_min}..{x_max}] y[{y_min}..{y_max}] z[{z_min}..{z_max}]"
    fig.text(0.5, 0.02, info, ha="center", fontsize=8, color="#777")

    fig.patch.set_facecolor(bg_color)
    ax.set_facecolor(bg_color)

    fig.savefig(output_path, dpi=dpi, facecolor=fig.get_facecolor())
    plt.close(fig)
    return output_path
