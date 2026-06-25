"""Block dataclass and color palette.

A Block is a single voxel: (x, y, z, block_type).
The block_type string is looked up in BLOCK_COLORS to get an RGB color
for rendering. Block types not in the palette fall back to a muted gray.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Tuple


@dataclass(frozen=True)
class Block:
    """A single Minecraft block at integer coordinates."""
    x: int
    y: int
    z: int
    block_type: str

    def key(self) -> Tuple[int, int, int]:
        """Position key, used for set-based diffing."""
        return (self.x, self.y, self.z)

    def as_tuple(self) -> Tuple[int, int, int, str]:
        """Full tuple for equality comparison."""
        return (self.x, self.y, self.z, self.block_type)


# RGB normalized (0..1) colors for each block type.
# Colors are approximate but distinct enough for visual identification.
BLOCK_COLORS: dict[str, Tuple[float, float, float]] = {
    # Terrain
    "stone":          (0.50, 0.50, 0.50),
    "cobblestone":    (0.42, 0.42, 0.42),
    "mossy_cobble":   (0.36, 0.45, 0.32),
    "mossy_bricks":   (0.36, 0.44, 0.32),
    "stone_bricks":   (0.48, 0.48, 0.48),
    "cracked_bricks": (0.40, 0.40, 0.40),
    "dirt":           (0.55, 0.40, 0.25),
    "grass_block":    (0.45, 0.62, 0.30),
    "sand":           (0.86, 0.80, 0.58),
    "red_sand":       (0.70, 0.40, 0.25),
    "sandstone":      (0.88, 0.82, 0.60),
    "red_sandstone":  (0.70, 0.40, 0.25),
    "gravel":         (0.45, 0.40, 0.38),
    "clay":           (0.55, 0.55, 0.62),
    "bedrock":        (0.25, 0.25, 0.25),
    "deepslate":      (0.32, 0.32, 0.34),
    "tuff":           (0.42, 0.42, 0.44),
    "calcite":        (0.86, 0.84, 0.80),
    "dripstone":      (0.55, 0.42, 0.32),

    # Wood
    "oak_log":        (0.55, 0.42, 0.22),
    "oak_planks":     (0.72, 0.58, 0.36),
    "oak_leaves":     (0.32, 0.50, 0.22),
    "spruce_log":     (0.32, 0.22, 0.14),
    "spruce_planks":  (0.42, 0.30, 0.18),
    "spruce_leaves":  (0.24, 0.36, 0.22),
    "birch_log":      (0.85, 0.85, 0.78),
    "birch_planks":   (0.86, 0.78, 0.60),
    "dark_oak_log":   (0.28, 0.18, 0.10),
    "dark_oak_planks":(0.30, 0.20, 0.12),
    "dark_oak_leaves":(0.20, 0.32, 0.16),

    # Ores / minerals
    "coal_ore":       (0.36, 0.36, 0.36),
    "iron_ore":       (0.62, 0.52, 0.42),
    "gold_ore":       (0.78, 0.62, 0.30),
    "diamond_ore":    (0.42, 0.66, 0.66),
    "emerald_ore":    (0.42, 0.62, 0.42),
    "lapis_ore":      (0.30, 0.42, 0.62),
    "redstone_ore":   (0.55, 0.30, 0.30),
    "iron_block":     (0.86, 0.86, 0.88),
    "gold_block":     (0.95, 0.82, 0.20),
    "diamond_block":  (0.40, 0.86, 0.84),
    "emerald_block":  (0.20, 0.78, 0.42),
    "lapis_block":    (0.16, 0.30, 0.66),
    "redstone_block": (0.62, 0.10, 0.10),
    "netherite_block":(0.32, 0.28, 0.26),

    # Decorative stone
    "bricks":         (0.62, 0.38, 0.32),
    "quartz":         (0.92, 0.90, 0.86),
    "obsidian":       (0.10, 0.05, 0.18),
    "crying_obsidian":(0.20, 0.10, 0.28),
    "netherrack":     (0.42, 0.20, 0.20),
    "nether_bricks":  (0.22, 0.10, 0.12),
    "basalt":         (0.22, 0.22, 0.24),
    "blackstone":     (0.18, 0.16, 0.20),
    "end_stone":      (0.86, 0.84, 0.62),
    "end_stone_bricks":(0.84, 0.82, 0.60),
    "purpur":         (0.66, 0.50, 0.66),
    "purpur_pillar":  (0.62, 0.46, 0.62),
    "prismarine":     (0.36, 0.50, 0.46),
    "prismarine_dark":(0.24, 0.36, 0.34),
    "prismarine_bricks":(0.40, 0.56, 0.52),

    # Glass / lights
    "glass":          (0.78, 0.86, 0.92),
    "glass_pane":     (0.78, 0.86, 0.92),
    "glowstone":      (0.92, 0.78, 0.40),
    "sea_lantern":    (0.78, 0.92, 0.92),
    "lantern":        (0.78, 0.62, 0.30),
    "torch":          (0.95, 0.78, 0.30),
    "shroomlight":    (0.95, 0.62, 0.40),
    "end_rod":        (0.95, 0.95, 0.78),
    "soul_lantern":   (0.42, 0.42, 0.62),
    "soul_torch":     (0.42, 0.42, 0.78),
    "magma":          (0.62, 0.24, 0.16),

    # Functional
    "bookshelf":      (0.62, 0.46, 0.28),
    "crafting_table": (0.62, 0.42, 0.22),
    "furnace":        (0.40, 0.40, 0.40),
    "chest":          (0.62, 0.46, 0.22),
    "ender_chest":    (0.20, 0.20, 0.22),
    "enchant_table":  (0.30, 0.20, 0.40),
    "anvil":          (0.30, 0.30, 0.32),
    "cauldron":       (0.30, 0.30, 0.32),
    "brewing_stand":  (0.30, 0.30, 0.32),
    "spawner":        (0.40, 0.30, 0.30),
    "end_portal_frame":(0.10, 0.20, 0.18),
    "bone_block":     (0.86, 0.84, 0.72),
    "cave_air":       (0.05, 0.05, 0.05),   # invisible (treated as transparent)
    "iron_chain":     (0.45, 0.45, 0.47),
    "wall_torch":     (0.95, 0.78, 0.30),

    # Liquids
    "water":          (0.20, 0.36, 0.78),
    "lava":           (0.92, 0.42, 0.10),

    # Vegetation
    "tall_grass":     (0.42, 0.58, 0.24),
    "flower_red":     (0.78, 0.20, 0.20),
    "flower_yellow":  (0.92, 0.82, 0.20),
    "cactus":         (0.36, 0.50, 0.24),
    "vine":           (0.24, 0.42, 0.18),
    "mushroom_red":   (0.78, 0.30, 0.30),
    "mushroom_brown": (0.62, 0.50, 0.36),
    "pumpkin":        (0.92, 0.62, 0.16),
    "jack_o_lantern": (0.92, 0.62, 0.16),
    "melon":          (0.55, 0.78, 0.36),
    "wheat":          (0.78, 0.66, 0.30),
    "chorus_plant":   (0.50, 0.32, 0.56),
    "chorus_flower":  (0.78, 0.50, 0.86),

    # Wool / concrete
    "wool_white":     (0.92, 0.92, 0.92),
    "wool_red":       (0.66, 0.20, 0.20),
    "wool_blue":      (0.30, 0.30, 0.66),
    "wool_purple":    (0.58, 0.30, 0.70),
    "wool_yellow":    (0.86, 0.78, 0.20),
    "carpet_red":     (0.66, 0.20, 0.20),
    "concrete_white": (0.92, 0.92, 0.92),
    "terracotta":     (0.62, 0.42, 0.32),

    # Rails / redstone
    "rails":          (0.62, 0.62, 0.62),
    "powered_rail":   (0.86, 0.62, 0.20),
    "redstone_wire":  (0.62, 0.10, 0.10),
    "redstone_torch": (0.86, 0.20, 0.10),
    "repeater":       (0.86, 0.62, 0.20),
    "lever":          (0.62, 0.46, 0.28),

    # Sculk / deep dark
    "sculk":          (0.10, 0.18, 0.22),
    "sculk_sensor":   (0.18, 0.30, 0.36),
    "sculk_shrieker": (0.14, 0.22, 0.28),
    "sculk_catalyst": (0.16, 0.24, 0.30),

    # Misc
    "oak_fence":      (0.72, 0.58, 0.36),
    "oak_door":       (0.72, 0.58, 0.36),
    "spruce_fence":   (0.42, 0.30, 0.18),
    "iron_door":      (0.86, 0.86, 0.88),
    "iron_bars":      (0.78, 0.78, 0.80),
    "ladder":         (0.62, 0.46, 0.28),
    "web":            (0.78, 0.78, 0.78),
    "snow":           (0.94, 0.95, 0.97),
    "ice":            (0.62, 0.78, 0.92),
    "blue_ice":       (0.40, 0.62, 0.86),
    "packed_ice":     (0.55, 0.72, 0.88),
    "sponge":         (0.92, 0.82, 0.36),
    "hay_bale":       (0.78, 0.62, 0.24),
    "target":         (0.92, 0.92, 0.92),
    "campfire":       (0.40, 0.30, 0.20),
    "soul_sand":      (0.32, 0.24, 0.20),
    "soul_soil":      (0.30, 0.22, 0.20),
    "nether_wart":    (0.66, 0.20, 0.24),
    "bell":           (0.78, 0.50, 0.16),
    "lectern":        (0.62, 0.46, 0.28),
    "barrel":         (0.62, 0.46, 0.28),
    "respawn_anchor": (0.40, 0.10, 0.20),
    "lodestone":      (0.40, 0.36, 0.32),
    "end_gateway":    (0.10, 0.10, 0.12),
}


def get_color(block_type: str) -> Tuple[float, float, float]:
    """Get RGB color for a block type. Falls back to muted gray."""
    return BLOCK_COLORS.get(block_type, (0.55, 0.45, 0.45))
