#pragma once

// 1:1 port of the PURE static math of
// net.minecraft.client.renderer.texture.OverlayTexture (Minecraft 26.1.2).
//
// OverlayTexture packs the entity damage/white-flash overlay coordinate that the
// renderer feeds into shaders. Its public static surface is plain integer/float
// arithmetic — a float->int narrowing and a couple of bit ops — and is ported here
// verbatim. The instance side (the 16x16 DynamicTexture built in the constructor and
// uploaded to the GPU) is NOT portable and is listed as unported below.
//
// Source (OverlayTexture.java):
//
//   public static final int NO_WHITE_U = 0;          // :9
//   public static final int RED_OVERLAY_V = 3;       // :10
//   public static final int WHITE_OVERLAY_V = 10;    // :11
//   public static final int NO_OVERLAY = pack(0, 10);// :12
//
//   public static int u(final float whiteOverlayProgress) {   // :37-39
//      return (int)(whiteOverlayProgress * 15.0F);
//   }
//   public static int v(final boolean hurtOverlay) {          // :41-43
//      return hurtOverlay ? 3 : 10;
//   }
//   public static int pack(final int u, final int v) {        // :45-47
//      return u | v << 16;
//   }
//   public static int pack(final float whiteOverlayProgress, final boolean redOverlay) { // :49-51
//      return pack(u(whiteOverlayProgress), v(redOverlay));
//   }
//
// 1:1 TRAPS captured exactly:
//   * `(int)(whiteOverlayProgress * 15.0F)` is a Java float->int narrowing conversion
//     (JLS 5.1.3): truncate toward zero; NaN -> 0; values >= Integer.MAX_VALUE ->
//     Integer.MAX_VALUE; values <= Integer.MIN_VALUE -> Integer.MIN_VALUE. A bare C++
//     `static_cast<int>(float)` is undefined behaviour for out-of-range inputs, so the
//     conversion is done through javaF2I() which reproduces Java's saturating semantics.
//     The multiply `whiteOverlayProgress * 15.0F` is a plain 32-bit float multiply.
//   * `u | v << 16` is Java operator precedence: `u | (v << 16)` on 32-bit ints, with
//     the shift performed in int (wrapping) arithmetic.
//
// SKIPPED (no portable numeric algorithm): the OverlayTexture() constructor's
// DynamicTexture allocation + per-pixel fill + GPU upload, close(), getTextureView().
// These touch NativeImage / GpuTextureView and carry no exposed pure math beyond what is
// ported above. Listed as unported.
//
// Certified bit-for-bit by overlay_texture_parity (ground truth:
// tools/OverlayTextureParity.java driving the REAL net.minecraft OverlayTexture).

#include <cmath>
#include <cstdint>
#include <limits>

namespace mc::render::overlaytexture {

// Java float->int narrowing conversion (JLS 5.1.3), matching the JVM's f2i:
//   NaN              -> 0
//   x >= 2^31        -> Integer.MAX_VALUE
//   x <= -2^31 - eps -> Integer.MIN_VALUE
//   otherwise        -> round toward zero
inline int javaF2I(float x) {
    if (std::isnan(x)) return 0;
    // 2147483648.0f is the exact float value 2^31. Anything >= it saturates high; the
    // smallest representable value below 2^31 (2147483520) still fits in int. Likewise
    // -2147483648.0f == -2^31 == INT_MIN exactly and is representable, so use a strict
    // comparison only on the high side.
    if (x >= 2147483648.0f) return std::numeric_limits<int>::max();
    if (x < -2147483648.0f) return std::numeric_limits<int>::min();
    return static_cast<int>(x);  // in-range: C++ truncation == Java truncation
}

// public static final int NO_WHITE_U = 0;             (OverlayTexture.java:9)
inline constexpr int NO_WHITE_U = 0;
// public static final int RED_OVERLAY_V = 3;          (OverlayTexture.java:10)
inline constexpr int RED_OVERLAY_V = 3;
// public static final int WHITE_OVERLAY_V = 10;       (OverlayTexture.java:11)
inline constexpr int WHITE_OVERLAY_V = 10;

// public static int u(final float whiteOverlayProgress) {
//    return (int)(whiteOverlayProgress * 15.0F);
// }
inline int u(float whiteOverlayProgress) {
    return javaF2I(whiteOverlayProgress * 15.0F);
}

// public static int v(final boolean hurtOverlay) {
//    return hurtOverlay ? 3 : 10;
// }
inline int v(bool hurtOverlay) {
    return hurtOverlay ? 3 : 10;
}

// public static int pack(final int u, final int v) {
//    return u | v << 16;   // == u | (v << 16)
// }
// Java << is on 32-bit ints and wraps; reproduce via unsigned shift then OR.
inline int pack(int u, int v) {
    return static_cast<int>(static_cast<uint32_t>(u) |
                            (static_cast<uint32_t>(v) << 16));
}

// public static int pack(final float whiteOverlayProgress, final boolean redOverlay) {
//    return pack(u(whiteOverlayProgress), v(redOverlay));
// }
inline int pack(float whiteOverlayProgress, bool redOverlay) {
    return pack(u(whiteOverlayProgress), v(redOverlay));
}

// public static final int NO_OVERLAY = pack(0, 10);   (OverlayTexture.java:12)
inline const int NO_OVERLAY = pack(0, 10);

}  // namespace mc::render::overlaytexture
