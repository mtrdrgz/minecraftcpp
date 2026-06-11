#pragma once
// 1:1 C++ port of the pure POSITIONING math of the GUI layout system
// (Minecraft Java Edition 26.1.2), net.minecraft.client.gui.layouts:
//   - LayoutSettings (LayoutSettingsImpl): padding + alignment-fraction builder
//   - LayoutElement / SpacerElement: a sized, positionable element
//   - AbstractLayout.AbstractChildWrapper: per-child cell positioning (getWidth/
//     getHeight + setX/setY) — NOTE the Java asymmetry: setX uses (int) TRUNCATION,
//     setY uses Math.round (preserved exactly).
//   - FrameLayout: max-size frame, centers/aligns each child, + the public static
//     alignInDimension / alignInRectangle / centerInRectangle helpers.
//
// Mth.lerp(float,float,float) = start + delta*(end-start), all in FLOAT. Certified by
// frame_layout_parity (drives the REAL FrameLayout / SpacerElement headless).
//
// UNPORTED (rendering/event coupled, not stubbed true): visitWidgets, render, the
// AbstractWidget element type, navigation focus, ScreenRectangle overloads.

#include <cstdint>
#include <vector>

#include "world/phys/shapes/JavaMath.h" // mc::jintCast / mc::javaMathRoundF

namespace mc::gui {

// Mth.lerp(float delta, float start, float end) — float arithmetic (Mth.java).
inline float mthLerpF(float delta, float start, float end) noexcept {
    return start + delta * (end - start);
}

// net.minecraft.client.gui.layouts.LayoutSettings.LayoutSettingsImpl — the padding +
// alignment-fraction builder (all setters return *this for chaining).
struct LayoutSettings {
    int paddingLeft = 0, paddingTop = 0, paddingRight = 0, paddingBottom = 0;
    float xAlignment = 0.0f, yAlignment = 0.0f;

    LayoutSettings& paddingLeftV(int p) { paddingLeft = p; return *this; }
    LayoutSettings& paddingTopV(int p) { paddingTop = p; return *this; }
    LayoutSettings& paddingRightV(int p) { paddingRight = p; return *this; }
    LayoutSettings& paddingBottomV(int p) { paddingBottom = p; return *this; }
    LayoutSettings& paddingHorizontal(int p) { return paddingLeftV(p).paddingRightV(p); }
    LayoutSettings& paddingVertical(int p) { return paddingTopV(p).paddingBottomV(p); }
    LayoutSettings& padding(int h, int v) { return paddingHorizontal(h).paddingVertical(v); }
    LayoutSettings& padding(int p) { return padding(p, p); }
    // Java padding(left, top, right, bottom): paddingLeft(left).paddingRight(right).paddingTop(top).paddingBottom(bottom)
    LayoutSettings& padding(int left, int top, int right, int bottom) {
        return paddingLeftV(left).paddingRightV(right).paddingTopV(top).paddingBottomV(bottom);
    }
    LayoutSettings& align(float ax, float ay) { xAlignment = ax; yAlignment = ay; return *this; }
    LayoutSettings& alignHorizontally(float ax) { xAlignment = ax; return *this; }
    LayoutSettings& alignVertically(float ay) { yAlignment = ay; return *this; }

    static LayoutSettings defaults() { return LayoutSettings{}; }
};

// net.minecraft.client.gui.layouts.LayoutElement — a sized, positionable element.
// SpacerElement is exactly this (fixed size, settable position).
struct LayoutElement {
    int x = 0, y = 0, width = 0, height = 0;
    LayoutElement() = default;
    LayoutElement(int w, int h) : width(w), height(h) {}
    LayoutElement(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getX() const { return x; }
    int getY() const { return y; }
    void setX(int nx) { x = nx; }
    void setY(int ny) { y = ny; }
};
using SpacerElement = LayoutElement;

// AbstractLayout.AbstractChildWrapper — the per-child cell positioning.
struct ChildWrapper {
    LayoutElement* child;
    LayoutSettings settings;

    // getWidth()  = child.getWidth()  + paddingLeft + paddingRight   (AbstractLayout.java:69-71)
    int getWidth() const { return child->getWidth() + settings.paddingLeft + settings.paddingRight; }
    // getHeight() = child.getHeight() + paddingTop  + paddingBottom  (AbstractLayout.java:65-67)
    int getHeight() const { return child->getHeight() + settings.paddingTop + settings.paddingBottom; }

    // setX(x, availableSpace) — offset = (int) lerp(xAlignment, paddingLeft, avail - childW - paddingRight).
    void setX(int x, int availableSpace) const {
        float leastOffset = (float)settings.paddingLeft;
        float mostOffset = (float)(availableSpace - child->getWidth() - settings.paddingRight);
        int offset = jintCast((double)mthLerpF(settings.xAlignment, leastOffset, mostOffset)); // (int) trunc
        child->setX(offset + x);
    }
    // setY(y, availableSpace) — offset = Math.round(lerp(yAlignment, paddingTop, avail - childH - paddingBottom)).
    void setY(int y, int availableSpace) const {
        float leastOffset = (float)settings.paddingTop;
        float mostOffset = (float)(availableSpace - child->getHeight() - settings.paddingBottom);
        int offset = javaMathRoundF(mthLerpF(settings.yAlignment, leastOffset, mostOffset)); // Math.round
        child->setY(offset + y);
    }
};

// net.minecraft.client.gui.layouts.FrameLayout.
class FrameLayout {
public:
    FrameLayout(int x = 0, int y = 0, int minWidth = 0, int minHeight = 0)
        : x_(x), y_(y), minWidth_(minWidth), minHeight_(minHeight) {}

    FrameLayout& setMinWidth(int w) { minWidth_ = w; return *this; }
    FrameLayout& setMinHeight(int h) { minHeight_ = h; return *this; }
    FrameLayout& setMinDimensions(int w, int h) { return setMinWidth(w).setMinHeight(h); }

    // Default child settings: align(0.5, 0.5) — FrameLayout.java:14.
    static LayoutSettings newChildLayoutSettings() { return LayoutSettings::defaults().align(0.5f, 0.5f); }

    LayoutElement* addChild(LayoutElement* child) { return addChild(child, newChildLayoutSettings()); }
    LayoutElement* addChild(LayoutElement* child, const LayoutSettings& s) {
        children_.push_back(ChildWrapper{child, s});
        return child;
    }

    // arrangeElements() — FrameLayout.java:51-69.
    void arrangeElements() {
        int resultWidth = minWidth_;
        int resultHeight = minHeight_;
        for (const ChildWrapper& c : children_) {
            resultWidth = c.getWidth() > resultWidth ? c.getWidth() : resultWidth;   // Math.max
            resultHeight = c.getHeight() > resultHeight ? c.getHeight() : resultHeight;
        }
        for (const ChildWrapper& c : children_) {
            c.setX(x_, resultWidth);
            c.setY(y_, resultHeight);
        }
        width_ = resultWidth;
        height_ = resultHeight;
    }

    int getX() const { return x_; }
    int getY() const { return y_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // ---- public static helpers (FrameLayout.java:89-111) ----

    // alignInDimension(pos, length, widgetLength, align) -> widget pos.
    // = pos + (int) lerp(align, 0.0F, length - widgetLength).
    static int alignInDimension(int pos, int length, int widgetLength, float align) {
        int offset = jintCast((double)mthLerpF(align, 0.0f, (float)(length - widgetLength)));
        return pos + offset;
    }
    // alignInRectangle: sets widget x/y. centerInRectangle = align 0.5,0.5.
    static void alignInRectangle(LayoutElement& w, int x, int y, int width, int height, float ax, float ay) {
        w.setX(alignInDimension(x, width, w.getWidth(), ax));
        w.setY(alignInDimension(y, height, w.getHeight(), ay));
    }
    static void centerInRectangle(LayoutElement& w, int x, int y, int width, int height) {
        alignInRectangle(w, x, y, width, height, 0.5f, 0.5f);
    }

private:
    int x_, y_, minWidth_, minHeight_;
    int width_ = 0, height_ = 0;
    std::vector<ChildWrapper> children_;
};

} // namespace mc::gui
