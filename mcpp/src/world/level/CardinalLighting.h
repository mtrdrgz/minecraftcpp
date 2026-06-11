#pragma once

// 1:1 port of net.minecraft.world.level.CardinalLighting (CardinalLighting.java) — the per-face
// directional block shading multipliers (the classic "down darker, sides medium, up full" look).
// Applied by ModelBlockRenderer/BlockModelLighter as ARGB.gray(byFace(direction)) (flat path) or
// scaleColor(byFace) (AO path). Pure float constants. Certified by cardinal_lighting_parity.
// Direction ordinals: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.

namespace mc::world::level {

namespace cardinal {

struct CardinalLighting {
    float down, up, north, south, west, east;

    // CardinalLighting.byFace(Direction): DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
    float byFace(int directionOrdinal) const {
        switch (directionOrdinal) {
            case 0: return down;
            case 1: return up;
            case 2: return north;
            case 3: return south;
            case 4: return west;
            default: return east;  // 5
        }
    }
};

// CardinalLighting.DEFAULT / NETHER (the only two presets).
inline constexpr CardinalLighting DEFAULT{0.5F, 1.0F, 0.8F, 0.8F, 0.6F, 0.6F};
inline constexpr CardinalLighting NETHER{0.9F, 0.9F, 0.8F, 0.8F, 0.6F, 0.6F};

}  // namespace cardinal

}  // namespace mc::world::level
