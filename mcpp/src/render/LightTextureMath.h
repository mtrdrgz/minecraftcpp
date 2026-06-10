#pragma once

// 1:1 port of the PURE brightness curve from
// net.minecraft.client.renderer.Lightmap (Minecraft 26.1.2 — this is the class
// historically named LightTexture).
//
//   public static float getBrightness(final DimensionType dimensionType, final int level) {
//      float v = level / 15.0F;
//      float curvedV = v / (4.0F - 3.0F * v);
//      return Mth.lerp(dimensionType.ambientLight(), curvedV, 1.0F);
//   }
//   (Lightmap.java:120-124)
//
// `dimensionType` is only read for its `ambientLight()` float (a record accessor on
// DimensionType — not portable here without bootstrapping the whole registry), so the
// pure math is exposed as getBrightness(ambientLight, level): the ambientLight float is
// passed in directly. The full Lightmap.getBrightness(DimensionType, int) composition
// is still certified end-to-end by the ground-truth tool, which feeds the REAL
// per-dimension ambientLight() values into BOTH the real method and this function.
//
// Everything is plain 32-bit float arithmetic (no libm, no (int)float narrowing):
//   level / 15.0F        — int->float promotion then float divide
//   v / (4.0F - 3.0F*v)  — float multiply/subtract/divide
//   Mth.lerp(a, p0, p1) = p0 + a * (p1 - p0)   (Mth.java:532-534, == mth::lerpF)
// so a faithful float replication is bit-exact. Certified by light_texture_parity
// (ground truth: tools/LightTextureParity.java vs the real net.minecraft classes).
//
// SKIPPED (texture-upload / GPU-state path, no pure math to port): the Lightmap
// constructor, render(LightmapRenderState) (Std140 UBO upload + render pass), close(),
// getTextureView(). These touch Blaze3D GpuTexture / RenderSystem and carry no
// portable numeric algorithm — listed as unported.

#include "../world/level/levelgen/Mth.h"

namespace mc::render::lighttexture {

namespace mth = mc::levelgen::mth;

// public static final int TEXTURE_SIZE = 16; (Lightmap.java:21)
inline constexpr int TEXTURE_SIZE = 16;

// Pure body of Lightmap.getBrightness(DimensionType, int), with the dimension's
// ambientLight() float passed in directly (see header comment).
//
//   float v = level / 15.0F;
//   float curvedV = v / (4.0F - 3.0F * v);
//   return Mth.lerp(ambientLight, curvedV, 1.0F);
inline float getBrightness(float ambientLight, int level) {
    float v = static_cast<float>(level) / 15.0F;
    float curvedV = v / (4.0F - 3.0F * v);
    return mth::lerpF(ambientLight, curvedV, 1.0F);
}

}  // namespace mc::render::lighttexture
