#pragma once

// 1:1 port of net.minecraft.client.renderer.LevelRenderer.getLightCoords(brightnessGetter, level,
// state, pos) (LevelRenderer.java:1441-1452) — the per-block packed light-coords used by both the
// flat and ambient-occlusion block lighting paths (ModelBlockRenderer / BlockModelLighter). Pure
// int math on the certified mc::util::lightcoords (block/withBlock). The level/state coupling is
// isolated to the 3 inputs (emissiveRendering, packedBrightness, lightEmission), fed externally.
// Certified by level_render_light_parity.

#include "../util/Brightness.h"

namespace mc::render {

namespace levelrender {

namespace lc = mc::util::lightcoords;

// 15728880 == 0xF000F0 == pack(block=15, sky=15) — emissive blocks render full-bright.
inline constexpr int FULL_BRIGHT = 15728880;

inline int getLightCoords(bool emissiveRendering, int packedBrightness, int lightEmission) {
    if (emissiveRendering) return FULL_BRIGHT;
    int block = lc::block(packedBrightness);
    return block < lightEmission ? lc::withBlock(packedBrightness, lightEmission) : packedBrightness;
}

}  // namespace levelrender

}  // namespace mc::render
