#pragma once

// Bit-exact 1:1 port of com.mojang.blaze3d.platform.Window#calculateScale
// (Minecraft Java Edition 26.1.2).
//
// Source (26.1.2/src/com/mojang/blaze3d/platform/Window.java, lines 445-463):
//
//   public int calculateScale(final int maxScale, final boolean enforceUnicode) {
//      int scale = 1;
//      while (
//         scale != maxScale
//            && scale < this.framebufferWidth
//            && scale < this.framebufferHeight
//            && this.framebufferWidth / (scale + 1) >= 320
//            && this.framebufferHeight / (scale + 1) >= 240
//      ) {
//         scale++;
//      }
//      if (enforceUnicode && scale % 2 != 0) {
//         scale++;
//      }
//      return scale;
//   }
//
// This is a public INSTANCE method on Window; it reads the instance fields
// framebufferWidth / framebufferHeight. Window construction requires a live
// GLFW context, so the method cannot be reached via reflection. We replicate
// the loop body verbatim as a pure function of (framebufferWidth,
// framebufferHeight, maxScale, enforceUnicode) and certify it against a Java
// harness (GuiScaleParity.java) that copies the identical body.
//
// All arithmetic is Java `int` (32-bit two's-complement). Java integer
// division truncates toward zero, matching C++ for both signs. No values are
// large enough to overflow given the framebuffer/scale guards, so plain int
// arithmetic is bit-exact with Java here.

namespace mc {
namespace client {

// 1:1 port of Window#calculateScale.
// maxScale corresponds to the guiScale option setting (0 == "auto").
inline int calculateScale(int framebufferWidth, int framebufferHeight, int maxScale, bool enforceUnicode) {
    int scale = 1;

    while (scale != maxScale
        && scale < framebufferWidth
        && scale < framebufferHeight
        && framebufferWidth / (scale + 1) >= 320
        && framebufferHeight / (scale + 1) >= 240) {
        scale++;
    }

    if (enforceUnicode && scale % 2 != 0) {
        scale++;
    }

    return scale;
}

} // namespace client
} // namespace mc
