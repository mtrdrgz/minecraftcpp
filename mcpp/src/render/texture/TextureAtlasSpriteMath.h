#pragma once

// 1:1 port of the PURE UV math of
// net.minecraft.client.renderer.texture.TextureAtlasSprite (Minecraft 26.1.2).
//
// A TextureAtlasSprite occupies a rectangle inside a stitched texture atlas. Its four
// normalized UV bounds (u0,u1,v0,v1) are computed in the constructor from the sprite's
// integer placement (x,y), its padding, its pixel size (contents.width()/height()), and
// the atlas dimensions (atlasWidth,atlasHeight). getU/getV then linearly interpolate a
// [0,1] offset across those bounds. This is the exact math that maps a model/quad UV onto
// the atlas, so byte-exact bounds => byte-exact sampling.
//
// Constructor (TextureAtlasSprite.java:26-38):
//   this.u0 = (float)(x + padding) / atlasWidth;
//   this.u1 = (float)(x + padding + contents.width())  / atlasWidth;
//   this.v0 = (float)(y + padding) / atlasHeight;
//   this.v1 = (float)(y + padding + contents.height()) / atlasHeight;
//
// getU/getV (TextureAtlasSprite.java:68-84):
//   public float getU(final float offset) { float diff = u1 - u0; return u0 + diff * offset; }
//   public float getV(final float offset) { float diff = v1 - v0; return v0 + diff * offset; }
//
// Numeric notes (everything is plain 32-bit float arithmetic; no libm, no contraction):
//   * `(x + padding + contents.width())` is computed in `int` first (Java int add, which
//     wraps on overflow), THEN cast `(float)`, THEN divided by the atlas dimension
//     (`int` promoted to `float`). The int adds are reproduced in `int` here.
//   * `(float)(intSum) / atlasWidth` — Java promotes `atlasWidth` (int) to float for the
//     divide, so we divide float-by-float. Reproduced with `static_cast<float>`.
//   * getU/getV are pure float subtract + fused-free multiply-add (a + b*c with -ffp-
//     contract=off, NOT std::fma) — matches the JVM which never contracts these.
//
// SKIPPED (no portable pure math): the SpriteContents / GPU-state members and every method
// that touches Blaze3D (uploadFirstFrame, uploadSpriteUbo's Std140 Matrix4f build,
// createAnimationState, wrap, close) — those carry texture-upload / atlas-UBO state, not a
// numeric algorithm reproducible without the GPU. uploadSpriteUbo's ortho2D/translate/scale
// matrices belong to the (separately gated) JOML layer and are not part of the UV math here.
//
// Certified bit-for-bit by texture_atlas_sprite_parity (ground truth:
// tools/TextureAtlasSpriteParity.java vs the REAL net.minecraft TextureAtlasSprite,
// constructed reflectively over a sweep of placements/sizes/atlas dimensions/offsets).

#include <cstdint>

namespace mc::render::texture {

// Pure value-type mirror of a TextureAtlasSprite's UV bounds. Construct from the same
// integer placement the real protected constructor takes; getU/getV match exactly.
struct TextureAtlasSpriteUv {
    float u0;
    float u1;
    float v0;
    float v1;

    // 1:1 of TextureAtlasSprite(... atlasWidth, atlasHeight, x, y, padding) UV computation,
    // with contents.width()/height() supplied directly as spriteWidth/spriteHeight.
    //
    //   this.u0 = (float)(x + padding) / atlasWidth;
    //   this.u1 = (float)(x + padding + contents.width()) / atlasWidth;
    //   this.v0 = (float)(y + padding) / atlasHeight;
    //   this.v1 = (float)(y + padding + contents.height()) / atlasHeight;
    static TextureAtlasSpriteUv make(int atlasWidth,
                                     int atlasHeight,
                                     int x,
                                     int y,
                                     int padding,
                                     int spriteWidth,
                                     int spriteHeight) {
        // Java evaluates the (x + padding [+ size]) sums as `int` (wrapping), then casts to
        // float, then divides by the int atlas dim (promoted to float for the divide).
        TextureAtlasSpriteUv s;
        s.u0 = static_cast<float>(x + padding) / static_cast<float>(atlasWidth);
        s.u1 = static_cast<float>(x + padding + spriteWidth) / static_cast<float>(atlasWidth);
        s.v0 = static_cast<float>(y + padding) / static_cast<float>(atlasHeight);
        s.v1 = static_cast<float>(y + padding + spriteHeight) / static_cast<float>(atlasHeight);
        return s;
    }

    // public float getU(final float offset) { float diff = u1 - u0; return u0 + diff * offset; }
    float getU(float offset) const {
        float diff = u1 - u0;
        return u0 + diff * offset;
    }

    // public float getV(final float offset) { float diff = v1 - v0; return v0 + diff * offset; }
    float getV(float offset) const {
        float diff = v1 - v0;
        return v0 + diff * offset;
    }

    float getU0() const { return u0; }
    float getU1() const { return u1; }
    float getV0() const { return v0; }
    float getV1() const { return v1; }
};

}  // namespace mc::render::texture
