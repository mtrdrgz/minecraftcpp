#pragma once

// 1:1 port of the GL-free row/layout geometry of net.minecraft.client.gui.components.
// AbstractSelectionList (+ its inner Entry) — the scrolling list behind options / world-select /
// server-list / resource-pack screens. Builds on the certified scroll math (ScrollArea.h /
// scroll_area_parity). Certified by selection_list_parity. The extract*/blit render path is
// GL-coupled and omitted; only entry positioning + hit-testing is modelled.
//
// 1:1 details captured here:
//   * Row band is centered + fixed width: getRowWidth()=220 (constant), getRowLeft()=getX()+width/2-
//     getRowWidth()/2 (integer /2 both places), getRowRight()=getRowLeft()+getRowWidth().
//   * getNextY()/repositionEntries() start at getFirstEntryY()=getY()+2 MINUS (int)scrollAmount()
//     (TRUNCATE the clamped double) and walk children by height. addEntry sets x/width to the row
//     band and y to getNextY() (i.e. appended at the running bottom), THEN setScrollAmount re-walks.
//   * contentHeight()=sum(childHeight)+4 (drives the inherited maxScrollAmount/scrollerHeight).
//   * scrollBarX() is OVERRIDDEN here: getRowRight()+scrollbarWidth()+2 (NOT the base
//     getRight()-scrollbarWidth()).
//   * Entry content insets by CONTENT_PADDING=2: contentX/Y=getX/Y+2, contentWidth/Height=getW/H-4,
//     middles use integer /2. Entry.isMouseOver = getRectangle().containsPoint((int)mx,(int)my)
//     (mouse doubles TRUNCATED to int, then half-open containment).

#include <algorithm>
#include <vector>

#include "../../world/level/levelgen/Mth.h"

namespace mc::gui {

namespace sellistmth = mc::levelgen::mth;

struct ListEntry {
    int x = 0, y = 0, width = 0, height = 0;

    int getX() const { return x; }
    int getY() const { return y; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    int getContentX() const { return getX() + 2; }
    int getContentY() const { return getY() + 2; }
    int getContentHeight() const { return getHeight() - 4; }
    int getContentYMiddle() const { return getContentY() + getContentHeight() / 2; }
    int getContentBottom() const { return getContentY() + getContentHeight(); }
    int getContentWidth() const { return getWidth() - 4; }
    int getContentXMiddle() const { return getContentX() + getContentWidth() / 2; }
    int getContentRight() const { return getContentX() + getContentWidth(); }

    // isMouseOver = getRectangle().containsPoint((int)mx, (int)my): truncate then half-open contains.
    bool isMouseOver(double mx, double my) const {
        int px = static_cast<int>(mx), py = static_cast<int>(my);
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

struct SelectionList {
    int x = 0, y = 0, width = 0, height = 0;
    int defaultEntryHeight = 0;
    double scrollAmount_ = 0.0;
    std::vector<ListEntry> children;

    int getX() const { return x; }
    int getY() const { return y; }
    int getRight() const { return x + width; }
    int getBottom() const { return y + height; }
    double scrollAmount() const { return scrollAmount_; }
    int scrollbarWidth() const { return 6; }  // ScrollbarSettings.scrollbarWidth (defaultSettings)

    int getRowWidth() const { return 220; }
    int getRowLeft() const { return getX() + width / 2 - getRowWidth() / 2; }
    int getRowRight() const { return getRowLeft() + getRowWidth(); }
    int getFirstEntryY() const { return getY() + 2; }

    int contentHeight() const {
        int total = 0;
        for (const auto& c : children) total += c.height;
        return total + 4;
    }
    int maxScrollAmount() const { return std::max(0, contentHeight() - height); }
    bool scrollable() const { return maxScrollAmount() > 0; }
    int scrollerHeight() const {
        int raw = static_cast<int>(static_cast<float>(height * height) / static_cast<float>(contentHeight()));
        return sellistmth::clamp(raw, 32, height - 8);
    }
    // OVERRIDE of AbstractScrollArea.scrollBarX().
    int scrollBarX() const { return getRowRight() + scrollbarWidth() + 2; }
    int scrollBarY() const {
        if (maxScrollAmount() == 0) return getY();
        int v = static_cast<int>(scrollAmount_) * (height - scrollerHeight()) / maxScrollAmount() + getY();
        return std::max(getY(), v);
    }

    int getNextY() const {
        int yy = getFirstEntryY() - static_cast<int>(scrollAmount());
        for (const auto& c : children) yy += c.height;
        return yy;
    }

    void repositionEntries() {
        int yy = getFirstEntryY() - static_cast<int>(scrollAmount());
        for (auto& c : children) {
            c.y = yy;
            yy += c.height;
            c.x = getRowLeft();
            c.width = getRowWidth();
        }
    }

    // setScrollAmount override: clamp (as doubles) then reposition.
    void setScrollAmount(double s) {
        scrollAmount_ = sellistmth::clamp(s, 0.0, static_cast<double>(maxScrollAmount()));
        repositionEntries();
    }
    void refreshScrollAmount() { setScrollAmount(scrollAmount_); }

    void setSize(int w, int h) { width = w; height = h; }
    void setPosition(int nx, int ny) { x = nx; y = ny; }
    void updateSizeAndPosition(int w, int h, int nx, int ny) {
        setSize(w, h);
        setPosition(nx, ny);
        repositionEntries();
        refreshScrollAmount();
    }

    // addEntry(entry, height): row-band x/width + appended-at-bottom y, then stored.
    void addEntry(int entryHeight) {
        ListEntry e;
        e.x = getRowLeft();
        e.width = getRowWidth();
        e.y = getNextY();
        e.height = entryHeight;
        children.push_back(e);
    }

    int getRowTop(int row) const { return children[row].y; }
    int getRowBottom(int row) const { return children[row].y + children[row].height; }

    // getEntryAtPosition: first child whose isMouseOver is true, else -1.
    int getEntryAtPosition(double px, double py) const {
        for (size_t i = 0; i < children.size(); ++i)
            if (children[i].isMouseOver(px, py)) return static_cast<int>(i);
        return -1;
    }
};

}  // namespace mc::gui
