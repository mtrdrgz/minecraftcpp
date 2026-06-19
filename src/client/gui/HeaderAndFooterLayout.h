#pragma once

// 1:1 port of net.minecraft.client.gui.layouts.HeaderAndFooterLayout.arrangeElements
// (HeaderAndFooterLayout.java) — the header / content / footer band layout behind nearly every modern
// menu screen (options, world-select, etc.). Pure integer geometry; composes the CERTIFIED
// FrameLayout arrange (GridLayout.h frameArrange). Certified by header_footer_layout_parity.
//
// 1:1 details captured here (verified against AbstractLayout/FrameLayout source):
//   * header frame: frameArrange at (0,0), minWidth=screenW, minHeight=headerHeight.
//   * footer frame: arrangeElements() at origin (0,0) THEN setY(screenH-footerHeight) — and
//     AbstractLayout.setY REPOSITIONS children by the (newY-oldY) delta, so this is exactly
//     frameArrange at (0, screenH-footerHeight), minWidth=screenW, minHeight=footerHeight.
//   * contents frame: arrange at (0,0) minWidth=screenW (minHeight defaults to 0, never set) THEN
//     setPosition(0, min(headerHeight+CONTENT_MARGIN_TOP, screenH-footerHeight-contentsFrameHeight)).
//     setPosition's setY delta-moves children, so == frameArrange at (0, contentY), minHeight=0.
//     contentsFrameHeight = the frame's RESOLVED height = max(0, max child wrapper height).
//   * CONTENT_MARGIN_TOP = 30; DEFAULT header/footer height = 33; all three frames center children at
//     align(0.5, 0.5) (FrameLayout default child settings).
//   * getContentHeight() = screenH - headerHeight - footerHeight (distinct from the contents FRAME's
//     resolved height used for maxContentY).

#include <algorithm>
#include <vector>

#include "GridLayout.h"

namespace mc::gui {

inline constexpr int HFL_CONTENT_MARGIN_TOP = 30;
inline constexpr int HFL_DEFAULT_HEADER_AND_FOOTER_HEIGHT = 33;

struct HeaderFooterResult {
    int headerHeight = 0;
    int footerHeight = 0;
    int contentHeight = 0;  // layout.getContentHeight()
    int footerY = 0;        // footer frame origin after setY
    int contentY = 0;       // contents frame origin after setPosition
};

// resolved frame height = max(minHeight, max child wrapper height); wrapper height = childH+padT+padB.
inline int hflResolvedHeight(int minHeight, const std::vector<GridChild>& children) {
    int h = minHeight;
    for (const GridChild& c : children) h = std::max(h, c.childH + c.padT + c.padB);
    return h;
}

// Fills outX/outY of each child vector; returns the layout-level heights + frame origins.
inline HeaderFooterResult headerAndFooterArrange(int screenW, int screenH, int headerHeight,
                                                 int footerHeight, std::vector<GridChild>& header,
                                                 std::vector<GridChild>& footer,
                                                 std::vector<GridChild>& contents) {
    HeaderFooterResult r;
    r.headerHeight = headerHeight;
    r.footerHeight = footerHeight;
    r.contentHeight = screenH - headerHeight - footerHeight;

    // header: origin (0,0), min (screenW, headerHeight).
    frameArrange(0, 0, screenW, headerHeight, header);

    // footer: arranged at origin then setY(screenH-footerHeight) == frameArrange at that Y.
    r.footerY = screenH - footerHeight;
    frameArrange(0, r.footerY, screenW, footerHeight, footer);

    // contents: minHeight defaults to 0; contentY = min(headerHeight+30, screenH-footerHeight-resolvedH).
    int contentsFrameHeight = hflResolvedHeight(0, contents);
    int preferredContentY = headerHeight + HFL_CONTENT_MARGIN_TOP;
    int maxContentY = screenH - footerHeight - contentsFrameHeight;
    r.contentY = std::min(preferredContentY, maxContentY);
    frameArrange(0, r.contentY, screenW, 0, contents);

    return r;
}

}  // namespace mc::gui
