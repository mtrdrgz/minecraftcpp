// Bit-exact 1:1 C++ port of the GUI-scale math in
// com.mojang.blaze3d.platform.Window (Minecraft 26.1.2).
//
// Source: 26.1.2/src/com/mojang/blaze3d/platform/Window.java
//   - public int calculateScale(int maxScale, boolean enforceUnicode)   (lines 445-463)
//   - public void setGuiScale(int guiScale)                             (lines 465-472)
//
// These two methods are pure deterministic integer/double arithmetic over the
// window's framebuffer dimensions — no GLFW, no GL, no native handles, no world
// state. calculateScale picks the largest integer GUI scale that keeps the
// virtual GUI surface at least BASE_WIDTH x BASE_HEIGHT (320x240); setGuiScale
// derives the scaled GUI surface size from the framebuffer size and the chosen
// scale.
//
// ── 1:1 traps faithfully reproduced ─────────────────────────────────────────
//   * calculateScale's while-condition is evaluated left-to-right with Java
//     short-circuit &&; each comparison is on the *candidate* scale: the
//     `framebufferWidth / (scale + 1) >= 320` tests use Java int division
//     (truncate toward zero). We reproduce the exact loop, including that the
//     guards use `scale + 1` while the loop bounds (`scale < framebufferWidth`)
//     use `scale`.
//   * The enforceUnicode rounding bumps an odd scale up by one *after* the loop
//     (`scale % 2 != 0` -> `scale++`), so the returned value can exceed maxScale
//     by one. Reproduced verbatim — never clamped.
//   * setGuiScale uses `double doubleGuiScale = guiScale;` then
//     `(int)(framebufferWidth / doubleGuiScale)` — an int/double division
//     (promotes the int numerator to double) truncated toward zero, and the
//     ceiling is taken by the comparison `framebufferWidth / doubleGuiScale >
//     width ? width + 1 : width`. The division is performed twice (once for the
//     truncation, once for the comparison) exactly as in the Java; with IEEE-754
//     doubles both evaluations are identical, so the comparison detects any
//     non-integer quotient. We keep both divisions to mirror the source.
//   * guiScale==0 would divide by zero; the real method is never called with 0
//     and we do not special-case it (Java would produce Infinity/(int)->0). Our
//     callers pass the calculateScale result (>= 1).
//
// Verbatim Java reference:
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
//   public void setGuiScale(final int guiScale) {
//      this.guiScale = guiScale;
//      double doubleGuiScale = guiScale;
//      int width = (int)(this.framebufferWidth / doubleGuiScale);
//      this.guiScaledWidth = this.framebufferWidth / doubleGuiScale > width ? width + 1 : width;
//      int height = (int)(this.framebufferHeight / doubleGuiScale);
//      this.guiScaledHeight = this.framebufferHeight / doubleGuiScale > height ? height + 1 : height;
//   }

#pragma once

#include <cstdint>

namespace mc::render {

// The pure GUI-scale math of com.mojang.blaze3d.platform.Window. Only the fields
// the two ported methods read/write are modelled: the framebuffer dimensions
// (inputs) and the derived guiScale / guiScaledWidth / guiScaledHeight (outputs).
class WindowGuiScale {
public:
    // public static final int BASE_WIDTH = 320;
    static constexpr int32_t BASE_WIDTH = 320;
    // public static final int BASE_HEIGHT = 240;
    static constexpr int32_t BASE_HEIGHT = 240;

    WindowGuiScale() = default;
    WindowGuiScale(int32_t framebufferWidth, int32_t framebufferHeight)
        : framebufferWidth_(framebufferWidth), framebufferHeight_(framebufferHeight) {}

    void setFramebufferSize(int32_t width, int32_t height) {
        framebufferWidth_ = width;
        framebufferHeight_ = height;
    }

    int32_t framebufferWidth() const { return framebufferWidth_; }
    int32_t framebufferHeight() const { return framebufferHeight_; }

    // public int calculateScale(int maxScale, boolean enforceUnicode)
    int32_t calculateScale(int32_t maxScale, bool enforceUnicode) const {
        int32_t scale = 1;

        while (scale != maxScale
               && scale < framebufferWidth_
               && scale < framebufferHeight_
               && framebufferWidth_ / (scale + 1) >= BASE_WIDTH      // Java int division
               && framebufferHeight_ / (scale + 1) >= BASE_HEIGHT) {
            scale++;
        }

        if (enforceUnicode && scale % 2 != 0) {
            scale++;
        }

        return scale;
    }

    // public void setGuiScale(int guiScale)
    void setGuiScale(int32_t guiScale) {
        guiScale_ = guiScale;
        double doubleGuiScale = static_cast<double>(guiScale);

        int32_t width = static_cast<int32_t>(framebufferWidth_ / doubleGuiScale);
        guiScaledWidth_ = (framebufferWidth_ / doubleGuiScale > width) ? width + 1 : width;

        int32_t height = static_cast<int32_t>(framebufferHeight_ / doubleGuiScale);
        guiScaledHeight_ = (framebufferHeight_ / doubleGuiScale > height) ? height + 1 : height;
    }

    int32_t getGuiScale() const { return guiScale_; }
    int32_t getGuiScaledWidth() const { return guiScaledWidth_; }
    int32_t getGuiScaledHeight() const { return guiScaledHeight_; }

private:
    int32_t framebufferWidth_ = 0;
    int32_t framebufferHeight_ = 0;
    int32_t guiScale_ = 0;
    int32_t guiScaledWidth_ = 0;
    int32_t guiScaledHeight_ = 0;
};

}  // namespace mc::render
