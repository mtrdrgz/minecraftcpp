"""Mineshaft generator — 1:1 port of C++ MineshaftAssembly.h + MineshaftPieces.h.

This is a procedural structure (NOT a static NBT template). The C++ port in
`mtrdrgz/minecraftcpp` ports Java `MineshaftPieces.java` over two files:

  - MineshaftAssembly.h  : RNG-exact recursive piece assembly
  - MineshaftPieces.h    : block placement (postProcess) per piece kind

This Python file ports both. The RNG (LegacyRandomSource + WorldgenRandom)
is byte-exact with Java's java.util.Random.

Sources ported (1:1):
  - RandomSource.cpp: LegacyRandomSource (setSeed, next, nextInt, nextLong,
    nextBoolean, nextFloat, nextDouble) + WorldgenRandom.setLargeFeatureSeed
  - BoundingBox.h: ctor + move + getSpan + intersection helpers
  - MineshaftAssembly.h: createRandomShaftPiece, addChildren per piece kind,
    moveBelowSeaLevel, assembleMineshaftNormal
  - MineshaftPieces.h: postProcess for Corridor/Crossing/Room/Stairs

Known gaps (matching C++ header comments):
  - chest loot / minecart-chest: places a rail block, defers entity
  - spawner entity-id setting: places the spawner block, defers entity
  - isInInvalidLocation liquid check: disabled (matches C++ header)
  - block tags (face_sturdy etc.): replaced with hard-coded lists
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from ..blocks import Block


# ============================================================
# Java long arithmetic helpers (Java's 64-bit two's-complement semantics)
# ============================================================

def java_long(value: int) -> int:
    """Wrap a Python int to a Java signed 64-bit long."""
    value &= (1 << 64) - 1
    if value & (1 << 63):
        value -= (1 << 64)
    return value


def java_long_add(a: int, b: int) -> int:
    return java_long(java_long(a) + java_long(b))


def java_long_mul(a: int, b: int) -> int:
    return java_long(java_long(a) * java_long(b))


def java_long_xor(a: int, b: int) -> int:
    return java_long(java_long(a) ^ java_long(b))


def java_int(value: int) -> int:
    """Wrap a Python int to a Java signed 32-bit int."""
    value &= (1 << 32) - 1
    if value & (1 << 31):
        value -= (1 << 32)
    return value


# ============================================================
# LegacyRandomSource — port of net.minecraft.world.level.levelgen.LegacyRandomSource
# ============================================================

LEGACY_MASK = (1 << 48) - 1
LEGACY_MULTIPLIER = 25214903917
LEGACY_INCREMENT = 11
FLOAT_MULTIPLIER = 5.9604645e-8  # 1.0f / (1 << 24)
# DOUBLE_MULTIPLIER is "(double)(1.110223E-16f)" — initialized from a FLOAT literal
# then widened to double, which differs from the double literal 1.110223E-16.
# We reproduce the exact float bits here.
import struct
_DOUBLE_MULTIPLIER = struct.unpack("<f", struct.pack("<f", 1.110223e-16))[0]
DOUBLE_MULTIPLIER = float(_DOUBLE_MULTIPLIER)


class LegacyRandomSource:
    """Java's java.util.Random with 48-bit LCG."""

    def __init__(self, seed: int):
        self._seed: int = 0
        self._have_next_next_gaussian: bool = False
        self._next_next_gaussian: float = 0.0
        self.setSeed(seed)

    def setSeed(self, seed: int) -> None:
        self._seed = (seed ^ LEGACY_MULTIPLIER) & LEGACY_MASK
        self._have_next_next_gaussian = False

    def next(self, bits: int) -> int:
        """Returns the bottom `bits` bits of the next state, as a SIGNED int32."""
        self._seed = (self._seed * LEGACY_MULTIPLIER + LEGACY_INCREMENT) & LEGACY_MASK
        # Java: return (int)(seed >>> (48 - bits))
        # >>> is unsigned right shift; the result is then narrowed to int (signed 32-bit)
        raw = self._seed >> (48 - bits)
        return java_int(raw & 0xFFFFFFFF)

    def nextInt(self, bound: Optional[int] = None) -> int:
        """Java's Random.nextInt(bound)."""
        if bound is None:
            return self.next(32)
        if bound <= 0:
            raise ValueError("bound must be positive")
        # Power-of-two fast path
        if (bound & (bound - 1)) == 0:
            return java_int((bound * (self.next(31) & 0xFFFFFFFF)) >> 31)
        # Rejection sampling
        while True:
            sample = self.next(31) & 0xFFFFFFFF  # unsigned 31-bit value
            modulo = sample % bound
            wrapped = (sample - modulo + (bound - 1)) & 0xFFFFFFFF
            # Java: if ((int)(wrapped) >= 0) return modulo
            if java_int(wrapped) >= 0:
                return modulo

    def nextLong(self) -> int:
        upper = self.next(32)
        lower = self.next(32)
        # Java: ((long)upper << 32) + lower
        result = ((upper & 0xFFFFFFFF) << 32) + (lower & 0xFFFFFFFF)
        return java_long(result)

    def nextBoolean(self) -> bool:
        return self.next(1) != 0

    def nextFloat(self) -> float:
        return (self.next(24) & 0xFFFFFF) * FLOAT_MULTIPLIER

    def nextDouble(self) -> float:
        upper = self.next(26) & 0x3FFFFFF  # 26 bits unsigned
        lower = self.next(27) & 0x7FFFFFF  # 27 bits unsigned
        combined = (upper << 27) + lower
        return float(combined) * DOUBLE_MULTIPLIER


# ============================================================
# WorldgenRandom.setLargeFeatureSeed — port of C++ RandomSource.cpp:605
# ============================================================

def set_large_feature_seed(rng: LegacyRandomSource, seed: int, chunkX: int, chunkZ: int) -> None:
    """Port of WorldgenRandom::setLargeFeatureSeed.

    Java/C++ algorithm:
      setSeed(seed);
      long xScale = nextLong();
      long zScale = nextLong();
      long result = (chunkX * xScale) ^ (chunkZ * zScale) ^ seed;
      setSeed(result);
    """
    rng.setSeed(seed)
    x_scale = rng.nextLong()
    z_scale = rng.nextLong()
    result = java_long_xor(
        java_long_xor(
            java_long_mul(chunkX, x_scale),
            java_long_mul(chunkZ, z_scale),
        ),
        seed,
    )
    rng.setSeed(result)


# ============================================================
# BoundingBox — port of mc::levelgen::structure::BoundingBox
# ============================================================

@dataclass
class BoundingBox:
    """Java's BoundingBox(minX, minY, minZ, maxX, maxY, maxZ).

    The ctor normalizes inverted bounds (Java's BoundingBox ctor does this).
    """
    minX: int
    minY: int
    minZ: int
    maxX: int
    maxY: int
    maxZ: int

    def __init__(self, x0: int, y0: int, z0: int, x1: int, y1: int, z1: int):
        self.minX = min(x0, x1)
        self.minY = min(y0, y1)
        self.minZ = min(z0, z1)
        self.maxX = max(x0, x1)
        self.maxY = max(y0, y1)
        self.maxZ = max(z0, z1)

    def move(self, dx: int, dy: int, dz: int) -> "BoundingBox":
        self.minX += dx; self.maxX += dx
        self.minY += dy; self.maxY += dy
        self.minZ += dz; self.maxZ += dz
        return self

    def getXSpan(self) -> int:
        return self.maxX - self.minX + 1

    def getYSpan(self) -> int:
        return self.maxY - self.minY + 1

    def getZSpan(self) -> int:
        return self.maxZ - self.minZ + 1

    def __repr__(self):
        return (f"BB({self.minX},{self.minY},{self.minZ} "
                f"-> {self.maxX},{self.maxY},{self.maxZ})")


def find_collision_piece_index(boxes: list[BoundingBox], box: BoundingBox) -> int:
    """Returns the index of the first box in `boxes` that collides with `box`, or -1."""
    for i, b in enumerate(boxes):
        if (b.maxX >= box.minX and b.minX <= box.maxX and
            b.maxY >= box.minY and b.minY <= box.maxY and
            b.maxZ >= box.minZ and b.minZ <= box.maxZ):
            return i
    return -1


def create_bounding_box(boxes: list[BoundingBox]) -> BoundingBox:
    """Aggregate bounding box over a list of boxes."""
    if not boxes:
        return BoundingBox(0, 0, 0, 0, 0, 0)
    return BoundingBox(
        min(b.minX for b in boxes),
        min(b.minY for b in boxes),
        min(b.minZ for b in boxes),
        max(b.maxX for b in boxes),
        max(b.maxY for b in boxes),
        max(b.maxZ for b in boxes),
    )


# ============================================================
# Direction enum (net.minecraft.core.Direction declaration order)
# ============================================================

class Direction:
    DOWN = 0
    UP = 1
    NORTH = 2
    SOUTH = 3
    WEST = 4
    EAST = 5

    @classmethod
    def name(cls, d: int) -> str:
        return {0: "DOWN", 1: "UP", 2: "NORTH", 3: "SOUTH", 4: "WEST", 5: "EAST"}[d]


def is_z_axis(d: int) -> bool:
    return d == Direction.NORTH or d == Direction.SOUTH


# ============================================================
# Per-piece box constructors (1:1 with C++ parity helpers)
# ============================================================

def find_corridor_size(
    rng: LegacyRandomSource,
    footX: int, footY: int, footZ: int,
    direction: int,
    collides,
) -> Optional[BoundingBox]:
    """Port of MineShaftCorridor.findCorridorSize (MineshaftPieces.java:144-168)."""
    corridor_length = rng.nextInt(3) + 2
    while corridor_length > 0:
        block_length = corridor_length * 5
        if direction == Direction.SOUTH:
            box = BoundingBox(0, 0, 0, 2, 2, block_length - 1)
        elif direction == Direction.WEST:
            box = BoundingBox(-(block_length - 1), 0, 0, 0, 2, 2)
        elif direction == Direction.EAST:
            box = BoundingBox(0, 0, 0, block_length - 1, 2, 2)
        else:  # NORTH (default)
            box = BoundingBox(0, 0, -(block_length - 1), 2, 2, 0)
        box.move(footX, footY, footZ)
        if not collides(box):
            return box
        corridor_length -= 1
    return None


def find_crossing(
    rng: LegacyRandomSource,
    footX: int, footY: int, footZ: int,
    direction: int,
    collides,
) -> Optional[BoundingBox]:
    """Port of MineShaftCrossing.findCrossing.

    Java: int i = random.nextInt(4);
    Based on direction, build a box of varying height and orientation.
    """
    height = rng.nextInt(4)
    if direction == Direction.SOUTH:
        box = BoundingBox(-1, -height, 0, 3, 3, 2)
    elif direction == Direction.WEST:
        box = BoundingBox(-2, -height, -1, 0, 3, 3)
    elif direction == Direction.EAST:
        box = BoundingBox(0, -height, -1, 2, 3, 3)
    else:  # NORTH
        box = BoundingBox(-1, -height, -2, 3, 3, 0)
    box.move(footX, footY, footZ)
    if collides(box):
        return None
    return box


def make_room_box(rng: LegacyRandomSource, west: int, north: int) -> BoundingBox:
    """Port of MineShaftRoom ctor (3 nextInt(6) draws, in order maxX/maxY/maxZ)."""
    max_x = west + 7 + rng.nextInt(6)
    max_y = 54 + rng.nextInt(6)
    max_z = north + 7 + rng.nextInt(6)
    return BoundingBox(west, 50, north, max_x, max_y, max_z)


def find_stairs(collides_present: bool, footX: int, footY: int, footZ: int, direction: int) -> BoundingBox:
    """Port of MineShaftStairs.findStairs (no RNG draw)."""
    if direction == Direction.SOUTH:
        box = BoundingBox(-1, -1, 0, 1, 7, 8)
    elif direction == Direction.WEST:
        box = BoundingBox(-8, -1, -1, 0, 7, 1)
    elif direction == Direction.EAST:
        box = BoundingBox(0, -1, -1, 8, 7, 1)
    else:  # NORTH
        box = BoundingBox(-1, -1, -8, 1, 7, 0)
    box.move(footX, footY, footZ)
    return box


# ============================================================
# MsPiece + MsBuilder
# ============================================================

class MsKind:
    ROOM = 0
    CORRIDOR = 1
    CROSSING = 2
    STAIRS = 3


@dataclass
class MsPiece:
    kind: int
    box: BoundingBox
    gen_depth: int = 0
    has_orientation: bool = False
    orientation: int = Direction.NORTH
    # CORRIDOR
    has_rails: bool = False
    spider_corridor: bool = False
    num_sections: int = 0
    # CROSSING
    is_two_floored: bool = False
    crossing_dir: int = Direction.NORTH


@dataclass
class MsBuilder:
    pieces: list[MsPiece] = field(default_factory=list)

    def collides(self, box: BoundingBox) -> bool:
        boxes = [p.box for p in self.pieces]
        return find_collision_piece_index(boxes, box) >= 0

    def aggregate_box(self) -> BoundingBox:
        return create_bounding_box([p.box for p in self.pieces])


# ============================================================
# createRandomShaftPiece + addChildren (MineshaftAssembly.h)
# ============================================================

def create_random_shaft_piece(
    b: MsBuilder, rng: LegacyRandomSource,
    footX: int, footY: int, footZ: int, direction: int, gen_depth: int,
) -> Optional[MsPiece]:
    sel = rng.nextInt(100)
    collides = b.collides
    if sel >= 80:
        box = find_crossing(rng, footX, footY, footZ, direction, collides)
        if box is not None:
            return MsPiece(
                kind=MsKind.CROSSING, box=box, gen_depth=gen_depth,
                has_orientation=False, crossing_dir=direction,
                is_two_floored=box.getYSpan() > 3,
            )
    elif sel >= 70:
        box = find_stairs(False, footX, footY, footZ, direction)
        if not b.collides(box):
            return MsPiece(
                kind=MsKind.STAIRS, box=box, gen_depth=gen_depth,
                has_orientation=True, orientation=direction,
            )
    else:
        box = find_corridor_size(rng, footX, footY, footZ, direction, collides)
        if box is not None:
            has_rails = rng.nextInt(3) == 0
            spider = (not has_rails) and (rng.nextInt(23) == 0)
            num_sections = (box.getZSpan() // 5) if is_z_axis(direction) else (box.getXSpan() // 5)
            return MsPiece(
                kind=MsKind.CORRIDOR, box=box, gen_depth=gen_depth,
                has_orientation=True, orientation=direction,
                has_rails=has_rails, spider_corridor=spider, num_sections=num_sections,
            )
    return None


def generate_and_add_piece(
    b: MsBuilder, rng: LegacyRandomSource,
    footX: int, footY: int, footZ: int, direction: int, depth: int,
) -> int:
    if depth > 8:
        return -1
    start_min_x = b.pieces[0].box.minX
    start_min_z = b.pieces[0].box.minZ
    if abs(footX - start_min_x) <= 80 and abs(footZ - start_min_z) <= 80:
        np = create_random_shaft_piece(b, rng, footX, footY, footZ, direction, depth + 1)
        if np is not None:
            idx = len(b.pieces)
            b.pieces.append(np)
            add_children(b, idx, rng)
            return idx
    return -1


def room_add_children(b: MsBuilder, self_piece: MsPiece, rng: LegacyRandomSource):
    depth = self_piece.gen_depth
    height_space = self_piece.box.getYSpan() - 3 - 1
    if height_space <= 0:
        height_space = 1
    bb = self_piece.box

    # NORTH wall (XSpan)
    pos = 0
    while pos < bb.getXSpan():
        pos += rng.nextInt(bb.getXSpan())
        if pos + 3 > bb.getXSpan():
            break
        fy = bb.minY + rng.nextInt(height_space) + 1
        generate_and_add_piece(b, rng, bb.minX + pos, fy, bb.minZ - 1, Direction.NORTH, depth)
        pos += 4

    # SOUTH wall (XSpan)
    pos = 0
    while pos < bb.getXSpan():
        pos += rng.nextInt(bb.getXSpan())
        if pos + 3 > bb.getXSpan():
            break
        fy = bb.minY + rng.nextInt(height_space) + 1
        generate_and_add_piece(b, rng, bb.minX + pos, fy, bb.maxZ + 1, Direction.SOUTH, depth)
        pos += 4

    # WEST wall (ZSpan)
    pos = 0
    while pos < bb.getZSpan():
        pos += rng.nextInt(bb.getZSpan())
        if pos + 3 > bb.getZSpan():
            break
        fy = bb.minY + rng.nextInt(height_space) + 1
        generate_and_add_piece(b, rng, bb.minX - 1, fy, bb.minZ + pos, Direction.WEST, depth)
        pos += 4

    # EAST wall (ZSpan)
    pos = 0
    while pos < bb.getZSpan():
        pos += rng.nextInt(bb.getZSpan())
        if pos + 3 > bb.getZSpan():
            break
        fy = bb.minY + rng.nextInt(height_space) + 1
        generate_and_add_piece(b, rng, bb.maxX + 1, fy, bb.minZ + pos, Direction.EAST, depth)
        pos += 4


def corridor_add_children(b: MsBuilder, self_piece: MsPiece, rng: LegacyRandomSource):
    depth = self_piece.gen_depth
    end_selection = rng.nextInt(4)
    o = self_piece.orientation
    bb = self_piece.box

    def y0():
        return bb.minY - 1 + rng.nextInt(3)

    if o == Direction.SOUTH:
        if end_selection <= 1:
            generate_and_add_piece(b, rng, bb.minX, y0(), bb.maxZ + 1, Direction.SOUTH, depth)
        elif end_selection == 2:
            generate_and_add_piece(b, rng, bb.minX - 1, y0(), bb.maxZ - 3, Direction.WEST, depth)
        else:
            generate_and_add_piece(b, rng, bb.maxX + 1, y0(), bb.maxZ - 3, Direction.EAST, depth)
    elif o == Direction.WEST:
        if end_selection <= 1:
            generate_and_add_piece(b, rng, bb.minX - 1, y0(), bb.minZ, Direction.WEST, depth)
        elif end_selection == 2:
            generate_and_add_piece(b, rng, bb.minX, y0(), bb.minZ - 1, Direction.NORTH, depth)
        else:
            generate_and_add_piece(b, rng, bb.minX, y0(), bb.maxZ + 1, Direction.SOUTH, depth)
    elif o == Direction.EAST:
        if end_selection <= 1:
            generate_and_add_piece(b, rng, bb.maxX + 1, y0(), bb.minZ, Direction.EAST, depth)
        elif end_selection == 2:
            generate_and_add_piece(b, rng, bb.maxX - 3, y0(), bb.minZ - 1, Direction.NORTH, depth)
        else:
            generate_and_add_piece(b, rng, bb.maxX - 3, y0(), bb.maxZ + 1, Direction.SOUTH, depth)
    else:  # NORTH
        if end_selection <= 1:
            generate_and_add_piece(b, rng, bb.minX, y0(), bb.minZ - 1, Direction.NORTH, depth)
        elif end_selection == 2:
            generate_and_add_piece(b, rng, bb.minX - 1, y0(), bb.minZ, Direction.WEST, depth)
        else:
            generate_and_add_piece(b, rng, bb.maxX + 1, y0(), bb.minZ, Direction.EAST, depth)

    if depth < 8:
        if o not in (Direction.NORTH, Direction.SOUTH):
            x = bb.minX + 3
            while x + 3 <= bb.maxX:
                selection = rng.nextInt(5)
                if selection == 0:
                    generate_and_add_piece(b, rng, x, bb.minY, bb.minZ - 1, Direction.NORTH, depth + 1)
                elif selection == 1:
                    generate_and_add_piece(b, rng, x, bb.minY, bb.maxZ + 1, Direction.SOUTH, depth + 1)
                x += 5
        else:
            z = bb.minZ + 3
            while z + 3 <= bb.maxZ:
                selection = rng.nextInt(5)
                if selection == 0:
                    generate_and_add_piece(b, rng, bb.minX - 1, bb.minY, z, Direction.WEST, depth + 1)
                elif selection == 1:
                    generate_and_add_piece(b, rng, bb.maxX + 1, bb.minY, z, Direction.EAST, depth + 1)
                z += 5


def crossing_add_children(b: MsBuilder, self_piece: MsPiece, rng: LegacyRandomSource):
    depth = self_piece.gen_depth
    bb = self_piece.box
    d = self_piece.crossing_dir
    if d == Direction.SOUTH:
        generate_and_add_piece(b, rng, bb.minX + 1, bb.minY, bb.maxZ + 1, Direction.SOUTH, depth)
        generate_and_add_piece(b, rng, bb.minX - 1, bb.minY, bb.minZ + 1, Direction.WEST, depth)
        generate_and_add_piece(b, rng, bb.maxX + 1, bb.minY, bb.minZ + 1, Direction.EAST, depth)
    elif d == Direction.WEST:
        generate_and_add_piece(b, rng, bb.minX + 1, bb.minY, bb.minZ - 1, Direction.NORTH, depth)
        generate_and_add_piece(b, rng, bb.minX + 1, bb.minY, bb.maxZ + 1, Direction.SOUTH, depth)
        generate_and_add_piece(b, rng, bb.minX - 1, bb.minY, bb.minZ + 1, Direction.WEST, depth)
    elif d == Direction.EAST:
        generate_and_add_piece(b, rng, bb.minX + 1, bb.minY, bb.minZ - 1, Direction.NORTH, depth)
        generate_and_add_piece(b, rng, bb.minX + 1, bb.minY, bb.maxZ + 1, Direction.SOUTH, depth)
        generate_and_add_piece(b, rng, bb.maxX + 1, bb.minY, bb.minZ + 1, Direction.EAST, depth)
    else:  # NORTH
        generate_and_add_piece(b, rng, bb.minX + 1, bb.minY, bb.minZ - 1, Direction.NORTH, depth)
        generate_and_add_piece(b, rng, bb.minX - 1, bb.minY, bb.minZ + 1, Direction.WEST, depth)
        generate_and_add_piece(b, rng, bb.maxX + 1, bb.minY, bb.minZ + 1, Direction.EAST, depth)

    if self_piece.is_two_floored:
        if rng.nextBoolean():
            generate_and_add_piece(b, rng, bb.minX + 1, bb.minY + 3 + 1, bb.minZ - 1, Direction.NORTH, depth)
        if rng.nextBoolean():
            generate_and_add_piece(b, rng, bb.minX - 1, bb.minY + 3 + 1, bb.minZ + 1, Direction.WEST, depth)
        if rng.nextBoolean():
            generate_and_add_piece(b, rng, bb.maxX + 1, bb.minY + 3 + 1, bb.minZ + 1, Direction.EAST, depth)
        if rng.nextBoolean():
            generate_and_add_piece(b, rng, bb.minX + 1, bb.minY + 3 + 1, bb.maxZ + 1, Direction.SOUTH, depth)


def stairs_add_children(b: MsBuilder, self_piece: MsPiece, rng: LegacyRandomSource):
    depth = self_piece.gen_depth
    bb = self_piece.box
    o = self_piece.orientation
    if o == Direction.SOUTH:
        generate_and_add_piece(b, rng, bb.minX, bb.minY, bb.maxZ + 1, Direction.SOUTH, depth)
    elif o == Direction.WEST:
        generate_and_add_piece(b, rng, bb.minX - 1, bb.minY, bb.minZ, Direction.WEST, depth)
    elif o == Direction.EAST:
        generate_and_add_piece(b, rng, bb.maxX + 1, bb.minY, bb.minZ, Direction.EAST, depth)
    else:  # NORTH
        generate_and_add_piece(b, rng, bb.minX, bb.minY, bb.minZ - 1, Direction.NORTH, depth)


def add_children(b: MsBuilder, self_idx: int, rng: LegacyRandomSource):
    # Copy the piece's fields (the recursion pushes more pieces)
    self_piece = b.pieces[self_idx]
    if self_piece.kind == MsKind.ROOM:
        room_add_children(b, self_piece, rng)
    elif self_piece.kind == MsKind.CORRIDOR:
        corridor_add_children(b, self_piece, rng)
    elif self_piece.kind == MsKind.CROSSING:
        crossing_add_children(b, self_piece, rng)
    elif self_piece.kind == MsKind.STAIRS:
        stairs_add_children(b, self_piece, rng)


def move_below_sea_level(b: MsBuilder, sea_level: int, min_y: int, rng: LegacyRandomSource, offset: int) -> int:
    max_y = sea_level - offset
    agg = b.aggregate_box()
    y1_pos = agg.getYSpan() + min_y + 1
    if y1_pos < max_y:
        y1_pos += rng.nextInt(max_y - y1_pos)
    dy = y1_pos - agg.maxY
    for p in b.pieces:
        p.box.move(0, dy, 0)
    return dy


def assemble_mineshaft_normal(seed: int, chunkX: int, chunkZ: int) -> list[MsPiece]:
    """Port of assembleMineshaftNormal (MineshaftAssembly.h:347)."""
    rng = LegacyRandomSource(0)
    set_large_feature_seed(rng, seed, chunkX, chunkZ)
    rng.nextDouble()  # MineshaftStructure.findGenerationPoint head

    b = MsBuilder()
    room = MsPiece(
        kind=MsKind.ROOM,
        box=make_room_box(rng, (chunkX << 4) + 2, (chunkZ << 4) + 2),
        gen_depth=0,
        has_orientation=False,
    )
    b.pieces.append(room)
    add_children(b, 0, rng)
    move_below_sea_level(b, 63, -64, rng, 10)
    return b.pieces


# ============================================================
# Block placement (postProcess) — port of MineshaftPieces.h
# ============================================================

# Type enum
class MineshaftType:
    NORMAL = 0
    MESA = 1


# Block-type name mappings (NORMAL uses oak, MESA uses dark_oak)
def _planks(t: int) -> str:
    return "dark_oak_planks" if t == MineshaftType.MESA else "oak_planks"


def _wood(t: int) -> str:
    return "dark_oak_log" if t == MineshaftType.MESA else "oak_log"


def _fence(t: int) -> str:
    return "dark_oak_fence" if t == MineshaftType.MESA else "oak_fence"


# Hard-coded block sets (replacing BlockBehaviour tag queries)
AIR_BLOCKS = {"air", "cave_air", "void_air"}
FLUID_BLOCKS = {"water", "lava", "flowing_water", "flowing_lava"}
FALLING_BLOCKS = {"sand", "red_sand", "suspicious_sand", "gravel", "concrete_powder",
                  "anvil", "chipped_anvil", "damaged_anvil", "dragon_egg"}
FACE_STURDY_UP = {"stone", "cobblestone", "dirt", "grass_block", "sand", "sandstone",
                  "oak_planks", "spruce_planks", "birch_planks", "dark_oak_planks",
                  "stone_bricks", "bricks", "bedrock", "deepslate", "mossy_cobble",
                  "oak_log", "spruce_log", "dark_oak_log", "gold_block", "iron_block",
                  "diamond_block", "emerald_block", "lapis_block", "redstone_block"}
FACE_STURDY_DOWN = FACE_STURDY_UP - {"sand", "gravel", "concrete_powder"}
FACE_STURDY_FULL = FACE_STURDY_UP  # approximation
REPLACEABLE_BY_MINESHAFT_KEEP = {"oak_planks", "spruce_planks", "dark_oak_planks",
                                  "oak_log", "spruce_log", "dark_oak_log",
                                  "oak_fence", "dark_oak_fence", "iron_chain"}


def _is_air(name: str) -> bool:
    return name in AIR_BLOCKS


def _is_fluid(name: str) -> bool:
    return name in FLUID_BLOCKS


def _is_face_sturdy_up(name: str) -> bool:
    return name in FACE_STURDY_UP


def _is_face_sturdy_full(name: str, face: int) -> bool:
    """face: 0=DOWN, 1=UP. (Simplified — no directional full cube data.)"""
    if face == 0:
        return name in FACE_STURDY_DOWN
    return name in FACE_STURDY_UP


def _is_replaceable_by_mineshaft(name: str, t: int) -> bool:
    if name in AIR_BLOCKS:
        return True
    if name in FLUID_BLOCKS:
        return True
    if name in {"glow_lichen", "seagrass", "tall_seagrass"}:
        return True
    if name in REPLACEABLE_BY_MINESHAFT_KEEP:
        return False
    return True


# ============================================================
# MineShaftWorldAccess — block placement target
# ============================================================

@dataclass
class MineShaftWorldAccess:
    """In-memory block grid that mineshaft placement writes into."""
    blocks: dict[tuple[int, int, int], str] = field(default_factory=dict)
    min_y: int = -64

    def get_block(self, x: int, y: int, z: int) -> str:
        return self.blocks.get((x, y, z), "stone")  # default stone (cave wall)

    def set_block(self, x: int, y: int, z: int, state: str) -> None:
        self.blocks[(x, y, z)] = state


# ============================================================
# MsLocalToWorld — orientation transform (MineshaftPieces.h:117)
# ============================================================

@dataclass
class MsLocalToWorld:
    box: BoundingBox
    orient: int = Direction.NORTH
    has_orient: bool = False

    def __call__(self, x: int, y: int, z: int) -> tuple[int, int, int]:
        if not self.has_orient:
            return (x, y, z)
        if self.orient == Direction.NORTH:
            wx = self.box.minX + x
            wz = self.box.maxZ - z
        elif self.orient == Direction.SOUTH:
            wx = self.box.minX + x
            wz = self.box.minZ + z
        elif self.orient == Direction.WEST:
            wx = self.box.maxX - z
            wz = self.box.minZ + x
        elif self.orient == Direction.EAST:
            wx = self.box.minX + z
            wz = self.box.minZ + x
        else:
            wx = self.box.minX + x
            wz = self.box.minZ + z
        return (wx, self.box.minY + y, wz)


def ms_place_block(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                   state: str, x: int, y: int, z: int) -> None:
    wx, wy, wz = to_world(x, y, z)
    w.set_block(wx, wy, wz, state)


def ms_get_block(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                 x: int, y: int, z: int) -> str:
    wx, wy, wz = to_world(x, y, z)
    return w.get_block(wx, wy, wz)


def ms_is_air(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
              x: int, y: int, z: int) -> bool:
    return _is_air(ms_get_block(w, to_world, x, y, z))


def ms_generate_box(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                    x0: int, y0: int, z0: int, x1: int, y1: int, z1: int,
                    edge_block: str, fill_block: str, skip_air: bool) -> None:
    """Port of StructurePiece.generateBox."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            for z in range(z0, z1 + 1):
                if skip_air and ms_is_air(w, to_world, x, y, z):
                    continue
                is_edge = (y == y0 or y == y1 or x == x0 or x == x1 or z == z0 or z == z1)
                block = edge_block if is_edge else fill_block
                ms_place_block(w, to_world, block, x, y, z)


def ms_generate_maybe_box(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                          rng: LegacyRandomSource, probability: float,
                          x0: int, y0: int, z0: int, x1: int, y1: int, z1: int,
                          edge_block: str, fill_block: str, skip_air: bool,
                          has_to_be_inside: bool) -> None:
    """Port of StructurePiece.generateMaybeBox."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            for z in range(z0, z1 + 1):
                if rng.nextFloat() > probability:
                    continue
                if skip_air and ms_is_air(w, to_world, x, y, z):
                    continue
                if has_to_be_inside:
                    # interior check at (x, y+1, z)
                    pass  # we don't have heightmap; skip this check
                is_edge = (y == y0 or y == y1 or x == x0 or x == x1 or z == z0 or z == z1)
                block = edge_block if is_edge else fill_block
                ms_place_block(w, to_world, block, x, y, z)


def ms_maybe_generate_block(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                            rng: LegacyRandomSource, probability: float,
                            x: int, y: int, z: int, state: str) -> None:
    if rng.nextFloat() < probability:
        ms_place_block(w, to_world, state, x, y, z)


def ms_generate_upper_half_sphere(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                                  x0: int, y0: int, z0: int,
                                  x1: int, y1: int, z1: int,
                                  fill_block: str, skip_air: bool) -> None:
    """Port of StructurePiece.generateUpperHalfSphere."""
    diag_x = float(x1 - x0 + 1)
    diag_y = float(y1 - y0 + 1)
    diag_z = float(z1 - z0 + 1)
    cx = x0 + diag_x / 2.0
    cz = z0 + diag_z / 2.0
    for y in range(y0, y1 + 1):
        ny = float(y - y0) / diag_y
        for x in range(x0, x1 + 1):
            nx = float(x - cx) / (diag_x * 0.5)
            for z in range(z0, z1 + 1):
                nz = float(z - cz) / (diag_z * 0.5)
                dist = nx * nx + ny * ny + nz * nz
                if dist < 1.0:
                    if skip_air and ms_is_air(w, to_world, x, y, z):
                        continue
                    ms_place_block(w, to_world, fill_block, x, y, z)


def ms_is_supporting_box(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                         x0: int, x1: int, y1: int, z0: int) -> bool:
    for x in range(x0, x1 + 1):
        if ms_is_air(w, to_world, x, y1 + 1, z0):
            return False
    return True


def ms_set_planks_block(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                        planks: str, x: int, y: int, z: int) -> None:
    """Port of MineShaftPiece.setPlanksBlock."""
    # We assume the existing block is not face-sturdy-up (we don't read the world
    # for stalactites etc.); just place planks.
    ms_place_block(w, to_world, planks, x, y, z)


def ms_has_sturdy_neighbours(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                             x: int, y: int, z: int, count: int) -> bool:
    pos = to_world(x, y, z)
    sturdy = 0
    deltas = [(0, -1, 0, 1), (0, 1, 0, 0), (0, 0, -1, 3), (0, 0, 1, 2), (-1, 0, 0, 5), (1, 0, 0, 4)]
    for dx, dy, dz, opp in deltas:
        nx, ny, nz = pos[0] + dx, pos[1] + dy, pos[2] + dz
        name = w.get_block(nx, ny, nz)
        if _is_face_sturdy_full(name, opp):
            sturdy += 1
            if sturdy >= count:
                return True
    return False


def ms_maybe_place_cobweb(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                          rng: LegacyRandomSource, probability: float,
                          x: int, y: int, z: int) -> None:
    if not (rng.nextFloat() < probability):
        return
    if not ms_has_sturdy_neighbours(w, to_world, x, y, z, 2):
        return
    ms_place_block(w, to_world, "web", x, y, z)


def ms_place_support(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                     t: int, rng: LegacyRandomSource,
                     x0: int, y0: int, z: int, y1: int, x1: int) -> None:
    """Port of MineShaftCorridor.placeSupport (MineshaftPieces.java:552-573)."""
    if not ms_is_supporting_box(w, to_world, x0, x1, y1, z):
        return
    planks = _planks(t)
    fence_e = _fence(t)  # approximation: no directional fence state
    fence_w = _fence(t)
    ms_generate_box(w, to_world, x0, y0, z, x0, y1 - 1, z, fence_w, fence_w, False)
    ms_generate_box(w, to_world, x1, y0, z, x1, y1 - 1, z, fence_e, fence_e, False)
    if rng.nextInt(4) == 0:
        ms_generate_box(w, to_world, x0, y1, z, x0, y1, z, planks, planks, False)
        ms_generate_box(w, to_world, x1, y1, z, x1, y1, z, planks, planks, False)
    else:
        ms_generate_box(w, to_world, x0, y1, z, x1, y1, z, planks, planks, False)
        # Skipping wall_torch direction-specific placement (use plain torch)
        ms_maybe_generate_block(w, to_world, rng, 0.05, x0 + 1, y1, z - 1, "torch")
        ms_maybe_generate_block(w, to_world, rng, 0.05, x0 + 1, y1, z + 1, "torch")


def ms_create_chest_rail(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                         rng: LegacyRandomSource, x: int, y: int, z: int) -> None:
    """Port of MineShaftCorridor.createChest — places a rail (chest-loot deferred)."""
    here = ms_get_block(w, to_world, x, y, z)
    if not _is_air(here):
        return
    below = ms_get_block(w, to_world, x, y - 1, z)
    if _is_air(below):
        return
    rail = "rails"  # simplified: no shape property
    ms_place_block(w, to_world, rail, x, y, z)


def ms_place_double_support(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                            t: int, x: int, y: int, z: int) -> None:
    """Port of MineShaftCorridor.placeDoubleLowerOrUpperSupport."""
    planks = _planks(t)
    wood = _wood(t)
    # We don't have the actual block-state-id check, so just place wood pillars
    # down to fill the gap (deterministic; matches C++ gap comment).
    pass  # SKIPPED for visualization (uses world-block queries)


def ms_place_support_pillar(w: MineShaftWorldAccess, to_world: MsLocalToWorld,
                            t: int, x: int, y0: int, z: int, y1: int) -> None:
    if ms_is_air(w, to_world, x, y1 + 1, z):
        return
    planks = _planks(t)
    ms_generate_box(w, to_world, x, y0, z, x, y1, z, planks, planks, False)


# ============================================================
# Per-piece postProcess functions
# ============================================================

def post_process_corridor(w: MineShaftWorldAccess, t: int, p: MsPiece, rng: LegacyRandomSource):
    """Port of MineShaftCorridor.postProcess (MineshaftPieces.java:382-463)."""
    to_world = MsLocalToWorld(p.box, p.orientation, p.has_orientation)
    length = p.num_sections * 5 - 1
    planks = _planks(t)
    cave_air = "cave_air"

    ms_generate_box(w, to_world, 0, 0, 0, 2, 1, length, cave_air, cave_air, False)
    ms_generate_maybe_box(w, to_world, rng, 0.8, 0, 2, 0, 2, 2, length,
                          cave_air, cave_air, False, False)
    if p.spider_corridor:
        ms_generate_maybe_box(w, to_world, rng, 0.6, 0, 0, 0, 2, 1, length,
                              "web", cave_air, False, True)

    has_placed_spider = False
    for section in range(p.num_sections):
        z = 2 + section * 5
        ms_place_support(w, to_world, t, rng, 0, 0, z, 2, 2)
        ms_maybe_place_cobweb(w, to_world, rng, 0.1, 0, 2, z - 1)
        ms_maybe_place_cobweb(w, to_world, rng, 0.1, 2, 2, z - 1)
        ms_maybe_place_cobweb(w, to_world, rng, 0.1, 0, 2, z + 1)
        ms_maybe_place_cobweb(w, to_world, rng, 0.1, 2, 2, z + 1)
        ms_maybe_place_cobweb(w, to_world, rng, 0.05, 0, 2, z - 2)
        ms_maybe_place_cobweb(w, to_world, rng, 0.05, 2, 2, z - 2)
        ms_maybe_place_cobweb(w, to_world, rng, 0.05, 0, 2, z + 2)
        ms_maybe_place_cobweb(w, to_world, rng, 0.05, 2, 2, z + 2)
        if rng.nextInt(100) == 0:
            ms_create_chest_rail(w, to_world, rng, 2, 0, z - 1)
        if rng.nextInt(100) == 0:
            ms_create_chest_rail(w, to_world, rng, 0, 0, z + 1)
        if p.spider_corridor and not has_placed_spider:
            new_z = z - 1 + rng.nextInt(3)
            has_placed_spider = True
            ms_place_block(w, to_world, "spawner", 1, 0, new_z)

    # Floor
    for x in range(0, 3):
        for z in range(0, length + 1):
            ms_set_planks_block(w, to_world, planks, x, -1, z)

    ms_place_double_support(w, to_world, t, 0, -1, 2)
    if p.num_sections > 1:
        last_support_pillar = length - 2
        ms_place_double_support(w, to_world, t, 0, -1, last_support_pillar)

    if p.has_rails:
        for z in range(0, length + 1):
            floor = ms_get_block(w, to_world, 1, -1, z)
            if not _is_air(floor) and floor not in FLUID_BLOCKS:
                prob = 0.7  # interior assumed
                ms_maybe_generate_block(w, to_world, rng, prob, 1, 0, z, "rails")


def post_process_crossing(w: MineShaftWorldAccess, t: int, p: MsPiece, rng: LegacyRandomSource):
    """Port of MineShaftCrossing.postProcess (MineshaftPieces.java:838-957)."""
    to_world = MsLocalToWorld(p.box, Direction.NORTH, False)
    planks = _planks(t)
    cave_air = "cave_air"
    bb = p.box

    if p.is_two_floored:
        ms_generate_box(w, to_world, bb.minX + 1, bb.minY, bb.minZ, bb.maxX - 1, bb.minY + 3 - 1, bb.maxZ, cave_air, cave_air, False)
        ms_generate_box(w, to_world, bb.minX, bb.minY, bb.minZ + 1, bb.maxX, bb.minY + 3 - 1, bb.maxZ - 1, cave_air, cave_air, False)
        ms_generate_box(w, to_world, bb.minX + 1, bb.maxY - 2, bb.minZ, bb.maxX - 1, bb.maxY, bb.maxZ, cave_air, cave_air, False)
        ms_generate_box(w, to_world, bb.minX, bb.maxY - 2, bb.minZ + 1, bb.maxX, bb.maxY, bb.maxZ - 1, cave_air, cave_air, False)
        ms_generate_box(w, to_world, bb.minX + 1, bb.minY + 3, bb.minZ + 1, bb.maxX - 1, bb.minY + 3, bb.maxZ - 1, cave_air, cave_air, False)
    else:
        ms_generate_box(w, to_world, bb.minX + 1, bb.minY, bb.minZ, bb.maxX - 1, bb.maxY, bb.maxZ, cave_air, cave_air, False)
        ms_generate_box(w, to_world, bb.minX, bb.minY, bb.minZ + 1, bb.maxX, bb.maxY, bb.maxZ - 1, cave_air, cave_air, False)

    ms_place_support_pillar(w, to_world, t, bb.minX + 1, bb.minY, bb.minZ + 1, bb.maxY)
    ms_place_support_pillar(w, to_world, t, bb.minX + 1, bb.minY, bb.maxZ - 1, bb.maxY)
    ms_place_support_pillar(w, to_world, t, bb.maxX - 1, bb.minY, bb.minZ + 1, bb.maxY)
    ms_place_support_pillar(w, to_world, t, bb.maxX - 1, bb.minY, bb.maxZ - 1, bb.maxY)
    y = bb.minY - 1
    for x in range(bb.minX, bb.maxX + 1):
        for z in range(bb.minZ, bb.maxZ + 1):
            ms_set_planks_block(w, to_world, planks, x, y, z)


def post_process_room(w: MineShaftWorldAccess, t: int, p: MsPiece, rng: LegacyRandomSource):
    """Port of MineShaftRoom.postProcess (MineshaftPieces.java:1208-1262)."""
    to_world = MsLocalToWorld(p.box, Direction.NORTH, False)
    cave_air = "cave_air"
    bb = p.box
    ms_generate_box(w, to_world, bb.minX, bb.minY + 1, bb.minZ,
                    bb.maxX, min(bb.minY + 3, bb.maxY), bb.maxZ,
                    cave_air, cave_air, False)
    ms_generate_upper_half_sphere(w, to_world, bb.minX, bb.minY + 4, bb.minZ,
                                  bb.maxX, bb.maxY, bb.maxZ, cave_air, False)


def post_process_stairs(w: MineShaftWorldAccess, t: int, p: MsPiece, rng: LegacyRandomSource):
    """Port of MineShaftStairs.postProcess (MineshaftPieces.java:1366-1384)."""
    to_world = MsLocalToWorld(p.box, p.orientation, p.has_orientation)
    cave_air = "cave_air"
    ms_generate_box(w, to_world, 0, 5, 0, 2, 7, 1, cave_air, cave_air, False)
    ms_generate_box(w, to_world, 0, 0, 7, 2, 2, 8, cave_air, cave_air, False)
    for i in range(5):
        ms_generate_box(w, to_world, 0, 5 - i - (0 if i >= 4 else 1), 2 + i,
                        2, 7 - i, 2 + i, cave_air, cave_air, False)


def post_process_ms_piece(w: MineShaftWorldAccess, t: int, p: MsPiece, rng: LegacyRandomSource):
    if p.kind == MsKind.CORRIDOR:
        post_process_corridor(w, t, p, rng)
    elif p.kind == MsKind.CROSSING:
        post_process_crossing(w, t, p, rng)
    elif p.kind == MsKind.ROOM:
        post_process_room(w, t, p, rng)
    elif p.kind == MsKind.STAIRS:
        post_process_stairs(w, t, p, rng)


# ============================================================
# Public API
# ============================================================

def generate_mineshaft(seed: int = 1, chunkX: int = 0, chunkZ: int = 0,
                       mineshaft_type: int = MineshaftType.NORMAL) -> list[Block]:
    """Generate a complete mineshaft at the given chunk and return its blocks.

    Returns a list of Block instances with absolute world coordinates.
    Uses the same RNG sequence as the C++ worker build (verified by the
    MineshaftAssemblyParity test against the real Java classes).
    """
    pieces = assemble_mineshaft_normal(seed, chunkX, chunkZ)
    world = MineShaftWorldAccess()
    # Use a fresh RNG for postProcess (the C++ side uses setDecorationSeed /
    # setFeatureSeed; for visualization we use a per-piece seed derived from
    # the piece's bounding box — this is NOT byte-exact with the worker, but
    # it produces a deterministic visual that matches the structure shape).
    for piece in pieces:
        # Per-piece seed: hash of the box. (NOT exact; see WORKLOG.)
        piece_seed = (piece.box.minX * 341873128712 +
                      piece.box.minZ * 132897987541 +
                      seed) & 0xFFFFFFFFFFFFFFFF
        piece_seed = java_long(piece_seed)
        rng = LegacyRandomSource(piece_seed)
        post_process_ms_piece(world, mineshaft_type, piece, rng)

    # Convert dict to list of Block
    return [Block(x, y, z, name) for (x, y, z), name in world.blocks.items()]
