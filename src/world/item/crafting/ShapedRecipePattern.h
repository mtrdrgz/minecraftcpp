// 1:1 port of the PURE coord/dimension/symmetry helpers of
//   net.minecraft.world.item.crafting.ShapedRecipePattern (Minecraft 26.1.2)
// plus net.minecraft.util.Util.isSymmetrical (the symmetry predicate the pattern
// ctor calls).
//
// Source:
//   26.1.2/src/net/minecraft/world/item/crafting/ShapedRecipePattern.java
//   26.1.2/src/net/minecraft/util/Util.java   (isSymmetrical, :498-517)
//
// SCOPE: only the self-contained string/coordinate math is ported here. Nothing in
// this header touches a world, level, registry, Ingredient, ItemStack, codec, or
// GL — it operates purely on a list of pattern rows (the recipe shape) and integer
// dimensions. The Ingredient-coupled `matches(CraftingInput)` is deliberately NOT
// ported; instead we expose its pure index helper `matchIndex(width,x,y,xFlip)`
// (ShapedRecipePattern.matches(..), :174-192) which is the byte-sensitive part.
//
// Ported surface:
//   * firstNonEmpty(line)            :136-144  leading-space scan -> first non-' ' index
//                                              (returns line.length() for all-space)
//   * lastNonEmpty(line)             :146-154  trailing-space scan -> last non-' ' index
//                                              (returns -1 for all-space / empty)
//   * shrink(pattern)                :101-134  trim surrounding blank rows/cols; the
//                                              top/bottom accumulation + empty-shape case
//   * matchIndex(width,x,y,xFlip)    :178-182  flat ingredient index, with the x-flip
//                                              mirror index `width - x - 1 + y*width`
//   * isSymmetrical(w,h,equalsAt)    Util:498  width==1 fast path + half-row mirror compare
//
// 1:1 TRAPS reproduced exactly:
//   - firstNonEmpty returns line.length() (NOT -1) for an all-space line, so a blank
//     row never lowers `left`; left starts at INT_MAX.
//   - lastNonEmpty returns -1 for all-space; right starts at 0, so an all-blank
//     pattern yields right==0 and width 1 unless every row is blank.
//   - shrink's top is only incremented while `top == i` (contiguous leading blanks);
//     bottom counts trailing blanks but RESETS to 0 on any non-blank row.
//   - Empty-shape special case: pattern.size() == bottom -> empty result.
//   - substring(left, right+1): right is the INCLUSIVE last column, so width = right-left+1.
//   - isSymmetrical: width==1 short-circuits true; centerX = width/2 (integer floor);
//     compares column leftX against width-1-leftX for each row.

#pragma once

#include <climits>
#include <functional>
#include <string>
#include <vector>

namespace mc::item::crafting {

// Java: ShapedRecipePattern.firstNonEmpty(String) :136-144
// Index of the first non-' ' char; line.length() if the line is all spaces/empty.
inline int firstNonEmpty(const std::string& line) {
    int index = 0;
    const int len = static_cast<int>(line.length());
    while (index < len && line[static_cast<size_t>(index)] == ' ') {
        index++;
    }
    return index;
}

// Java: ShapedRecipePattern.lastNonEmpty(String) :146-154
// Index of the last non-' ' char; -1 if the line is all spaces/empty.
inline int lastNonEmpty(const std::string& line) {
    int index = static_cast<int>(line.length()) - 1;
    while (index >= 0 && line[static_cast<size_t>(index)] == ' ') {
        index--;
    }
    return index;
}

// Java: ShapedRecipePattern.shrink(List<String>) :101-134
// Trims the blank margin around a pattern. Mirrors Java's int math exactly:
//   left starts at Integer.MAX_VALUE, right/top/bottom at 0.
// Returns the shrunk rows (an empty vector for an all-blank pattern).
inline std::vector<std::string> shrink(const std::vector<std::string>& pattern) {
    int left = INT_MAX;
    int right = 0;
    int top = 0;
    int bottom = 0;

    const int size = static_cast<int>(pattern.size());
    for (int i = 0; i < size; i++) {
        const std::string& line = pattern[static_cast<size_t>(i)];
        const int fne = firstNonEmpty(line);
        if (fne < left) left = fne;                 // Math.min(left, firstNonEmpty(line))
        const int lastNonSpace = lastNonEmpty(line);
        if (lastNonSpace > right) right = lastNonSpace; // Math.max(right, lastNonSpace)
        if (lastNonSpace < 0) {
            if (top == i) {
                top++;
            }
            bottom++;
        } else {
            bottom = 0;
        }
    }

    if (size == bottom) {
        return {};  // new String[0]
    }

    const int resultLen = size - bottom - top;
    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(resultLen));
    for (int line = 0; line < resultLen; line++) {
        // pattern.get(line + top).substring(left, right + 1)
        const std::string& row = pattern[static_cast<size_t>(line + top)];
        result.push_back(row.substr(static_cast<size_t>(left),
                                    static_cast<size_t>(right + 1 - left)));
    }
    return result;
}

// Java: ShapedRecipePattern.matches(CraftingInput, boolean) :178-182
// The flat ingredient index for cell (x,y) in a width*height pattern. With xFlip
// the column is mirrored (the un-symmetrical recipe's reflected match).
inline int matchIndex(int width, int x, int y, bool xFlip) {
    return xFlip ? (width - x - 1 + y * width)
                 : (x + y * width);
}

// Java: Util.isSymmetrical(width, height, List<T>) :498-517
// width==1 is always symmetrical; otherwise every row must read the same forwards
// and backwards. `equalsAt(a,b)` reports whether ingredient slot a equals slot b
// (the caller supplies the element-equality; here it is a pure index comparison).
inline bool isSymmetrical(int width, int height,
                          const std::function<bool(int, int)>& equalsAt) {
    if (width == 1) {
        return true;
    }
    const int centerX = width / 2;
    for (int y = 0; y < height; y++) {
        for (int leftX = 0; leftX < centerX; leftX++) {
            const int rightX = width - 1 - leftX;
            const int leftIdx = leftX + y * width;
            const int rightIdx = rightX + y * width;
            if (!equalsAt(leftIdx, rightIdx)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace mc::item::crafting
