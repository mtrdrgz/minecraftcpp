// 1:1 C++ port of the PURE 2D-grid helper nested in the REAL decompiled
// 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces
//     -> private static class SimpleGrid          (WoodlandMansionPieces.java:1157-1197)
//
// SimpleGrid is a fixed-size width*height grid of ints with an out-of-bounds
// sentinel `valueIfOutside`. It is completely self-contained PURE geometry:
//   * no world writes, no RandomSource, no registry/datapack, no Direction.
//   * backing store is a Java `new int[width][height]` — DEFAULT-ZERO-INITIALISED.
//
// Methods (all exact translations; line refs into WoodlandMansionPieces.java):
//   SimpleGrid(int width, int height, int valueIfOutside)   :1163-1168
//   void set(int x, int y, int value)                       :1170-1174  (silent OOB drop)
//   void set(int x0,int y0,int x1,int y1,int value)         :1176-1182  (<= bounds; reversed = no-op)
//   int  get(int x, int y)                                  :1184-1186  (OOB -> valueIfOutside)
//   void setif(int x,int y,int ifValue,int value)           :1188-1192
//   boolean edgesTo(int x,int y,int ifValue)                :1194-1196  (4-neighbour, leans on OOB get)
//
// 1:1 TRAPS this gate locks down:
//   - get()/set() bounds use `x>=0 && x<width && y>=0 && y<height`; OOB reads return
//     the sentinel and OOB writes are SILENTLY DROPPED (never clamp, never throw).
//   - the rectangle set() iterates y0..y1 then x0..x1 with `<=`; a reversed range
//     (x1<x0 or y1<y0) writes NOTHING (it must not be normalised).
//   - edgesTo() deliberately probes (x-1),(x+1),(y+1),(y-1) THROUGH get(), so at a
//     grid edge/corner it compares against `valueIfOutside`, not the in-grid value.
//   - Java `int` is 32-bit two's-complement; values flow through unchanged (no math
//     here can overflow because cells store whatever int is written verbatim).
//
// Indexing matches Java row-major `grid[x][y]`: we flatten as x*height + y.

#pragma once

#include <cstdint>
#include <vector>

namespace mc::levelgen::structure::structures {

class SimpleGrid {
public:
    // WoodlandMansionPieces.SimpleGrid(int,int,int) — :1163-1168.
    // Java `new int[width][height]` default-initialises every cell to 0.
    SimpleGrid(int width, int height, int valueIfOutside)
        : width_(width), height_(height), valueIfOutside_(valueIfOutside),
          grid_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0) {}

    // SimpleGrid.set(int x, int y, int value) — :1170-1174.
    void set(int x, int y, int value) {
        if (x >= 0 && x < width_ && y >= 0 && y < height_) {
            grid_[idx(x, y)] = value;
        }
    }

    // SimpleGrid.set(int x0, int y0, int x1, int y1, int value) — :1176-1182.
    void set(int x0, int y0, int x1, int y1, int value) {
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                set(x, y, value);
            }
        }
    }

    // SimpleGrid.get(int x, int y) — :1184-1186.
    int get(int x, int y) const {
        return (x >= 0 && x < width_ && y >= 0 && y < height_) ? grid_[idx(x, y)]
                                                               : valueIfOutside_;
    }

    // SimpleGrid.setif(int x, int y, int ifValue, int value) — :1188-1192.
    void setif(int x, int y, int ifValue, int value) {
        if (get(x, y) == ifValue) {
            set(x, y, value);
        }
    }

    // SimpleGrid.edgesTo(int x, int y, int ifValue) — :1194-1196.
    bool edgesTo(int x, int y, int ifValue) const {
        return get(x - 1, y) == ifValue || get(x + 1, y) == ifValue ||
               get(x, y + 1) == ifValue || get(x, y - 1) == ifValue;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    int valueIfOutside() const { return valueIfOutside_; }

private:
    std::size_t idx(int x, int y) const {
        // Mirrors Java grid[x][y]; only reached after the in-bounds check, so the
        // cast to size_t is always of a non-negative in-range index.
        return static_cast<std::size_t>(x) * static_cast<std::size_t>(height_) +
               static_cast<std::size_t>(y);
    }

    int width_;
    int height_;
    int valueIfOutside_;
    std::vector<int32_t> grid_;
};

} // namespace mc::levelgen::structure::structures
