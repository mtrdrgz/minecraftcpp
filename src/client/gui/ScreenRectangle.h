#pragma once

// 1:1 port of net.minecraft.client.gui.navigation.ScreenRectangle (ScreenRectangle.java) — the
// integer rectangle behind GUI widget bounds, hit-testing and keyboard navigation. Pure int math,
// no GL. Certified by screen_rectangle_parity.
//
// 1:1 detail: overlaps() uses INCLUSIVE high bounds (getBoundInDirection(positive) = coord+len-1,
// compared with <=), so touching edges count as overlap; intersects() uses STRICT < (touching edges
// do NOT intersect). These two are intentionally different.

#include <algorithm>

namespace mc::gui {

struct ScreenRectangle {
    int x = 0, y = 0, width = 0, height = 0;

    int top() const { return y; }
    int bottom() const { return y + height; }
    int left() const { return x; }
    int right() const { return x + width; }

    // overlapsInAxis(axis): max(lower bounds) <= min(higher bounds), higher = coord + length - 1.
    bool overlapsInAxisH(const ScreenRectangle& o) const {
        return std::max(left(), o.left()) <= std::min(x + width - 1, o.x + o.width - 1);
    }
    bool overlapsInAxisV(const ScreenRectangle& o) const {
        return std::max(top(), o.top()) <= std::min(y + height - 1, o.y + o.height - 1);
    }
    bool overlaps(const ScreenRectangle& o) const { return overlapsInAxisH(o) && overlapsInAxisV(o); }

    bool intersects(const ScreenRectangle& o) const {
        return left() < o.right() && right() > o.left() && top() < o.bottom() && bottom() > o.top();
    }
    bool encompasses(const ScreenRectangle& o) const {
        return o.left() >= left() && o.top() >= top() && o.right() <= right() && o.bottom() <= bottom();
    }
    bool containsPoint(int px, int py) const {
        return px >= left() && px < right() && py >= top() && py < bottom();
    }

    // intersection: writes `out` and returns true iff non-empty (Java returns the rect or null).
    bool intersection(const ScreenRectangle& o, ScreenRectangle& out) const {
        int l = std::max(left(), o.left());
        int t = std::max(top(), o.top());
        int r = std::min(right(), o.right());
        int b = std::min(bottom(), o.bottom());
        if (l < r && t < b) { out = ScreenRectangle{l, t, r - l, b - t}; return true; }
        return false;
    }
};

}  // namespace mc::gui
