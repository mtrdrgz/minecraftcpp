#pragma once

// 1:1 port of net.minecraft.client.gui.layouts.GridLayout.arrangeElements (GridLayout.java:229) +
// AbstractLayout.AbstractChildWrapper (the cell sizing / alignment / padding). Pure integer geometry
// — the foundation every modern GUI screen lays out with (LinearLayout wraps a 1xN / Nx1 grid). No
// GL. Composes the CERTIFIED Divisor (render/model/Divisor.h), Mth.lerp (mth::lerpF), and
// Math.round(float) (argb::javaRoundF). Certified by grid_layout_parity.
//
// 1:1 TRAPS captured:
//  - cell sizing uses the WRAPPER size (child + paddingL/R or T/B); but setX/setY's mostOffset uses
//    the RAW child size (availableSpace - child.getWidth() - paddingRight).
//  - setX offset = (int)Mth.lerp(...)  (TRUNCATE toward zero); setY offset = Math.round(Mth.lerp(...))
//    — DIFFERENT rounding per axis.

#include "../../render/model/Divisor.h"
#include "../../util/ARGB.h"
#include "../../world/level/levelgen/Mth.h"

#include <algorithm>
#include <vector>

namespace mc::gui {

namespace mth = mc::levelgen::mth;
namespace argb = mc::argb;
using mc::render::model::Divisor;

struct GridChild {
    int row = 0, col = 0, occRows = 1, occCols = 1;
    int childW = 0, childH = 0;
    int padL = 0, padT = 0, padR = 0, padB = 0;
    float alignX = 0.0f, alignY = 0.0f;
    int outX = 0, outY = 0;  // computed
};

// GridLayout.arrangeElements: positions each child given grid origin + row/column spacing.
inline void gridArrange(int gridX, int gridY, int rowSpacing, int columnSpacing,
                        std::vector<GridChild>& children) {
    int maxRow = 0, maxColumn = 0;
    for (const GridChild& c : children) {
        maxRow = std::max(c.row + c.occRows - 1, maxRow);
        maxColumn = std::max(c.col + c.occCols - 1, maxColumn);
    }

    std::vector<int> maxColumnWidths(maxColumn + 1, 0), maxRowHeights(maxRow + 1, 0);
    for (const GridChild& c : children) {
        int wrapperH = c.childH + c.padT + c.padB;                 // AbstractChildWrapper.getHeight()
        int childHeight = wrapperH - (c.occRows - 1) * rowSpacing;
        Divisor hd(childHeight, c.occRows);
        for (int row = c.row; row <= c.row + c.occRows - 1; ++row)
            maxRowHeights[row] = std::max(maxRowHeights[row], hd.nextInt());

        int wrapperW = c.childW + c.padL + c.padR;                 // AbstractChildWrapper.getWidth()
        int childWidth = wrapperW - (c.occCols - 1) * columnSpacing;
        Divisor wd(childWidth, c.occCols);
        for (int col = c.col; col <= c.col + c.occCols - 1; ++col)
            maxColumnWidths[col] = std::max(maxColumnWidths[col], wd.nextInt());
    }

    std::vector<int> columnXOffsets(maxColumn + 1, 0), rowYOffsets(maxRow + 1, 0);
    for (int col = 1; col <= maxColumn; ++col)
        columnXOffsets[col] = columnXOffsets[col - 1] + maxColumnWidths[col - 1] + columnSpacing;
    for (int row = 1; row <= maxRow; ++row)
        rowYOffsets[row] = rowYOffsets[row - 1] + maxRowHeights[row - 1] + rowSpacing;

    for (GridChild& c : children) {
        int availableWidth = 0;
        for (int col = c.col; col <= c.col + c.occCols - 1; ++col) availableWidth += maxColumnWidths[col];
        availableWidth += columnSpacing * (c.occCols - 1);
        int xBase = gridX + columnXOffsets[c.col];
        // AbstractChildWrapper.setX(xBase, availableWidth): (int)Mth.lerp(alignX, padL, avail-childW-padR)
        float leastX = static_cast<float>(c.padL);
        float mostX = static_cast<float>(availableWidth - c.childW - c.padR);
        int offX = static_cast<int>(mth::lerpF(c.alignX, leastX, mostX));
        c.outX = offX + xBase;

        int availableHeight = 0;
        for (int row = c.row; row <= c.row + c.occRows - 1; ++row) availableHeight += maxRowHeights[row];
        availableHeight += rowSpacing * (c.occRows - 1);
        int yBase = gridY + rowYOffsets[c.row];
        // setY: Math.round(Mth.lerp(alignY, padT, avail-childH-padB))
        float leastY = static_cast<float>(c.padT);
        float mostY = static_cast<float>(availableHeight - c.childH - c.padB);
        int offY = argb::javaRoundF(mth::lerpF(c.alignY, leastY, mostY));
        c.outY = offY + yBase;
    }
}

// 1:1 port of FrameLayout.arrangeElements (FrameLayout.java) — overlapping children, each aligned
// within the frame's resolved size (max of minWidth/Height and every child wrapper size). Reuses the
// same AbstractChildWrapper alignment (setX truncates, setY rounds). row/col/span on GridChild are
// ignored here.
inline void frameArrange(int frameX, int frameY, int minWidth, int minHeight,
                         std::vector<GridChild>& children) {
    int resultWidth = minWidth, resultHeight = minHeight;
    for (const GridChild& c : children) {
        resultWidth = std::max(resultWidth, c.childW + c.padL + c.padR);    // wrapper getWidth()
        resultHeight = std::max(resultHeight, c.childH + c.padT + c.padB);  // wrapper getHeight()
    }
    for (GridChild& c : children) {
        float leastX = static_cast<float>(c.padL);
        float mostX = static_cast<float>(resultWidth - c.childW - c.padR);
        c.outX = static_cast<int>(mth::lerpF(c.alignX, leastX, mostX)) + frameX;
        float leastY = static_cast<float>(c.padT);
        float mostY = static_cast<float>(resultHeight - c.childH - c.padB);
        c.outY = argb::javaRoundF(mth::lerpF(c.alignY, leastY, mostY)) + frameY;
    }
}

// 1:1 port of LinearLayout (LinearLayout.java) — a thin wrapper over a 1xN/Nx1 GridLayout:
// HORIZONTAL -> columnSpacing, child i at (row 0, col i); VERTICAL -> rowSpacing, child i at (row i,
// col 0); arrangeElements() delegates to the (certified) GridLayout arrange.
enum class Orientation { HORIZONTAL, VERTICAL };

inline void linearArrange(Orientation o, int x, int y, int spacing, std::vector<GridChild>& children) {
    int rowSpacing = (o == Orientation::VERTICAL) ? spacing : 0;
    int columnSpacing = (o == Orientation::HORIZONTAL) ? spacing : 0;
    for (size_t i = 0; i < children.size(); ++i) {
        children[i].occRows = 1;
        children[i].occCols = 1;
        if (o == Orientation::HORIZONTAL) { children[i].row = 0; children[i].col = static_cast<int>(i); }
        else { children[i].row = static_cast<int>(i); children[i].col = 0; }
    }
    gridArrange(x, y, rowSpacing, columnSpacing, children);
}

}  // namespace mc::gui
