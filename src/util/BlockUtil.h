// 1:1 C++ port of net.minecraft.util.BlockUtil (26.1.2) — the pure, self-contained
// pieces only. We deliberately port ONLY getMaxRectangleLocation (the
// @VisibleForTesting largest-rectangle-in-a-histogram helper) plus its IntBounds
// value type. The rest of BlockUtil (getLargestRectangleAround / getTopConnectedBlock)
// touches BlockGetter/BlockState/BlockPos and is intentionally NOT ported here.
//
// Source (decompiled Java, verbatim algorithm):
//   static Pair<IntBounds, Integer> getMaxRectangleLocation(int[] columns) {
//      int maxStart = 0, maxEnd = 0, maxHeight = 0;
//      IntStack stack = new IntArrayList();   // fastutil LIFO int stack
//      stack.push(0);
//      for (int column = 1; column <= columns.length; column++) {
//         int height = column == columns.length ? 0 : columns[column];
//         while (!stack.isEmpty()) {
//            int stackHeight = columns[stack.topInt()];
//            if (height >= stackHeight) { stack.push(column); break; }
//            stack.popInt();
//            int start = stack.isEmpty() ? 0 : stack.topInt() + 1;
//            if (stackHeight * (column - start) > maxHeight * (maxEnd - maxStart)) {
//               maxEnd = column; maxStart = start; maxHeight = stackHeight;
//            }
//         }
//         if (stack.isEmpty()) stack.push(column);
//      }
//      return new Pair(new IntBounds(maxStart, maxEnd - 1), maxHeight);
//   }
//
// 1:1 traps captured here:
//   * fastutil IntArrayList used as a LIFO stack: push=append, topInt=peek-last,
//     popInt=remove-last, isEmpty=size==0. The stack stores *column indices*, and
//     `columns[stack.topInt()]` re-reads the histogram height at that index — NOT a
//     cached height. Getting this wrong silently changes which bar bounds the rect.
//   * The area comparison is strict `>` (first-wins ties), and the products use
//     Java `int` arithmetic — so we use int32_t and let it wrap exactly as the JVM
//     would on overflow.
//   * Sentinel: the loop runs one past the end (column == length) with height 0 to
//     flush the remaining stack; and after the inner while, an empty stack is
//     re-seeded with the current column.
//   * Result is IntBounds(maxStart, maxEnd - 1) — the inclusive max index is
//     maxEnd-1, an off-by-one that the test exercises.

#pragma once

#include <cstdint>
#include <vector>

namespace mc::util::block_util {

// Mirrors net.minecraft.util.BlockUtil.IntBounds (public final int min, max).
struct IntBounds {
    int32_t min;
    int32_t max;

    constexpr IntBounds(int32_t mn, int32_t mx) : min(mn), max(mx) {}

    bool operator==(const IntBounds& o) const { return min == o.min && max == o.max; }
};

// Mirrors Pair<IntBounds, Integer> — the (bounds, height) result.
struct MaxRectangle {
    IntBounds bounds;
    int32_t height;

    bool operator==(const MaxRectangle& o) const {
        return bounds == o.bounds && height == o.height;
    }
};

// 1:1 port of BlockUtil.getMaxRectangleLocation(int[]).
inline MaxRectangle getMaxRectangleLocation(const std::vector<int32_t>& columns) {
    int32_t maxStart = 0;
    int32_t maxEnd = 0;
    int32_t maxHeight = 0;

    // fastutil IntStack backed by an int array; push/topInt/popInt/isEmpty.
    std::vector<int32_t> stack;
    stack.push_back(0);

    const int32_t len = static_cast<int32_t>(columns.size());
    for (int32_t column = 1; column <= len; ++column) {
        const int32_t height = (column == len) ? 0 : columns[static_cast<size_t>(column)];

        while (!stack.empty()) {
            const int32_t stackHeight = columns[static_cast<size_t>(stack.back())];  // columns[topInt()]
            if (height >= stackHeight) {
                stack.push_back(column);
                break;
            }

            stack.pop_back();  // popInt()
            const int32_t start = stack.empty() ? 0 : stack.back() + 1;  // topInt()+1
            // Strict `>`, Java int arithmetic (wrapping on overflow).
            if (stackHeight * (column - start) > maxHeight * (maxEnd - maxStart)) {
                maxEnd = column;
                maxStart = start;
                maxHeight = stackHeight;
            }
        }

        if (stack.empty()) {
            stack.push_back(column);
        }
    }

    return MaxRectangle{IntBounds(maxStart, maxEnd - 1), maxHeight};
}

}  // namespace mc::util::block_util
