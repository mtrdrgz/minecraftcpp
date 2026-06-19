#pragma once

// 1:1 port of the pure scroll / scrollbar math of net.minecraft.client.gui.components.
// AbstractScrollArea (AbstractScrollArea.java) — the GL-free foundation under every scrollable list
// (options, world-select, server list, resource packs, all AbstractSelectionList screens). Certified
// by scroll_area_parity. No GL: only the geometry + clamp surface is modelled (the extract*/blit
// render methods are GL-coupled and omitted).
//
// 1:1 details captured here:
//   * scrollerHeight() = Mth.clamp((int)((float)(height*height) / contentHeight()), 32, height-8):
//     int*int -> (float) -> divide by contentHeight (promoted to float) -> (int) TRUNCATE, then a
//     LITERAL-32 min (NOT the settings scrollbarMinHeight field) and a height-8 max that can fall
//     BELOW 32 for small heights — Mth.clamp(int)=min(max(v,min),max) then yields height-8.
//   * scrollBarY() does ALL-INTEGER arithmetic after (int)scrollAmount (truncating the clamped
//     double): (int)scrollAmount * (height - scrollerHeight()) / maxScrollAmount() + getY(), guarded
//     by Math.max(getY(), ...); the maxScrollAmount()==0 case returns getY() directly.
//   * setScrollAmount() clamps as DOUBLES: Mth.clamp(s, 0.0, (double)maxScrollAmount()).
//   * maxScrollAmount() = Math.max(0, contentHeight() - height) [int].
//   * isOverScrollbar uses INCLUSIVE x high edge (x <= scrollBarX()+scrollbarWidth()) but half-open
//     y (y < getBottom()).
//   * scrollBarX() here is the BASE class formula getRight()-scrollbarWidth(); AbstractSelectionList
//     OVERRIDES it (getRowRight()+scrollbarWidth()+2) — not modelled by this base gate.

#include <algorithm>

#include "../../world/level/levelgen/Mth.h"

namespace mc::gui {

namespace scrollmth = mc::levelgen::mth;

struct ScrollArea {
    int x = 0, y = 0, width = 0, height = 0;
    int contentHeight_ = 0;     // abstract contentHeight() in Java; supplied by the concrete subclass
    int scrollbarWidth_ = 6;    // ScrollbarSettings.scrollbarWidth (defaultSettings = 6)
    double scrollRate_ = 0.0;   // ScrollbarSettings.scrollRate (int in the record, returned as double)
    double scrollAmount_ = 0.0; // the clamped stored scroll position

    int getY() const { return y; }
    int getHeight() const { return height; }
    int getRight() const { return x + width; }
    int getBottom() const { return y + height; }

    int contentHeight() const { return contentHeight_; }
    int scrollbarWidth() const { return scrollbarWidth_; }
    double scrollRate() const { return scrollRate_; }
    double scrollAmount() const { return scrollAmount_; }

    int maxScrollAmount() const { return std::max(0, contentHeight() - height); }
    bool scrollable() const { return maxScrollAmount() > 0; }

    int scrollerHeight() const {
        int raw = static_cast<int>(static_cast<float>(height * height) / static_cast<float>(contentHeight()));
        return scrollmth::clamp(raw, 32, height - 8);
    }

    int scrollBarX() const { return getRight() - scrollbarWidth(); }

    int scrollBarY() const {
        if (maxScrollAmount() == 0) return getY();
        int v = static_cast<int>(scrollAmount_) * (height - scrollerHeight()) / maxScrollAmount() + getY();
        return std::max(getY(), v);
    }

    void setScrollAmount(double s) {
        scrollAmount_ = scrollmth::clamp(s, 0.0, static_cast<double>(maxScrollAmount()));
    }
    void refreshScrollAmount() { setScrollAmount(scrollAmount_); }

    bool isOverScrollbar(double px, double py) const {
        return px >= scrollBarX() && px <= scrollBarX() + scrollbarWidth() && py >= getY() && py < getBottom();
    }

    // mouseScrolled(mx, my, scrollX, scrollY) effect (visible==true): scrollAmount -= scrollY*scrollRate.
    void mouseScrolled(double scrollY) { setScrollAmount(scrollAmount() - scrollY * scrollRate()); }
};

}  // namespace mc::gui
