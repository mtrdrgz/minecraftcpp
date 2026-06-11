#pragma once

// 1:1 C++ port of the PURE grid-cleanup pass nested in the REAL decompiled
// 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces
//     -> private static class MansionGrid
//          static  boolean isHouse(SimpleGrid grid, int x, int y)   [:212-215 -> :211-214]
//          (instance) boolean cleanEdges(SimpleGrid grid)           [:212-241]
//
// These two helpers form the "thicken the floor plan" sweep that the mansion
// generator runs (`while (cleanEdges(baseGrid)) {}`) right after carving the
// corridor skeleton. They are fully self-contained integer geometry operating on
// a single SimpleGrid:
//   * NO world writes, NO RandomSource, NO registry/datapack, NO Direction.
//   * `cleanEdges` reads NO instance state of MansionGrid (it touches only its
//     `grid` argument and the static `isHouse`), so it ports as a free function.
//
// SimpleGrid (the int grid with an out-of-bounds sentinel) is reused verbatim
// from WoodlandMansionGrid.h, which is itself certified byte-exact by
// woodland_mansion_grid_parity.
//
// ── Algorithm (verbatim) ─────────────────────────────────────────────────────
//   isHouse(grid,x,y): v = grid.get(x,y); return v==1||v==2||v==3||v==4.
//     i.e. the cell is CORRIDOR(1)/ROOM(2)/START_ROOM(3)/TEST_ROOM(4); CLEAR(0),
//     BLOCKED(5), the room-flag composites, AND the OOB sentinel are all "not a
//     house". (For the real mansion grid valueIfOutside==5, so OOB is never a
//     house — but isHouse is sentinel-agnostic; we keep the literal 1..4 test.)
//
//   cleanEdges(grid): single full row-major sweep, y outer (0..height-1), x inner
//     (0..width-1). For each CLEAR cell (grid.get(x,y)==0):
//       directNeighbors = #house among the 4 orthogonal neighbours
//                         (+1,0)(-1,0)(0,+1)(0,-1), summed in THIS fixed order.
//       if directNeighbors >= 3:                 grid.set(x,y,2); touched=true;
//       else if directNeighbors == 2:
//          diagonalNeighbors = #house among the 4 diagonals
//                              (+1,+1)(-1,+1)(+1,-1)(-1,-1);
//          if diagonalNeighbors <= 1:            grid.set(x,y,2); touched=true;
//     return touched.
//
// ── 1:1 TRAPS this gate locks down ───────────────────────────────────────────
//   * IN-PLACE MUTATION DURING THE SWEEP: a cell promoted to ROOM(2) is read as a
//     "house" by isHouse for every LATER cell in the SAME pass (the +x and +y
//     neighbours of subsequent cells). The order — y outer, x inner, increasing —
//     is therefore load-bearing; a single grid is mutated as it is scanned. We do
//     NOT snapshot. (Driving cleanEdges repeatedly to a fixed point, as the real
//     ctor does, makes the order even more consequential.)
//   * BRANCH STRUCTURE: `>= 3` fills unconditionally; `== 2` fills only when the
//     diagonal count is `<= 1`; exactly 2 with >=2 diagonals, or <2 direct
//     neighbours, leaves the cell CLEAR. The two thresholds are independent.
//   * NEIGHBOUR PROBE ORDER & OOB: every neighbour query goes through SimpleGrid.get,
//     so edge/corner cells compare against `valueIfOutside`; with the canonical
//     sentinel 5 (BLOCKED, not in 1..4) OOB is never a house. We preserve the exact
//     get() bounds + sentinel semantics by reusing the gated SimpleGrid.
//   * Only CLEAR(0) cells are even considered; non-zero cells (incl. composites
//     with the room-type/flag bits set) are skipped — the `== 0` guard is exact.

#include "WoodlandMansionGrid.h"

namespace mc::levelgen::structure::structures {

// WoodlandMansionPieces.MansionGrid.isHouse(SimpleGrid, int, int).
// CORRIDOR=1, ROOM=2, START_ROOM=3, TEST_ROOM=4 are the four "house" cell values.
inline bool mansionGridIsHouse(const SimpleGrid& grid, int x, int y) {
    int value = grid.get(x, y);
    return value == 1 || value == 2 || value == 3 || value == 4;
}

// WoodlandMansionPieces.MansionGrid.cleanEdges(SimpleGrid).
// One full row-major (y outer, x inner) sweep, mutating `grid` in place. Returns
// true iff at least one CLEAR cell was promoted to ROOM(2) during this sweep.
inline bool mansionGridCleanEdges(SimpleGrid& grid) {
    bool touched = false;

    for (int y = 0; y < grid.height(); y++) {
        for (int x = 0; x < grid.width(); x++) {
            if (grid.get(x, y) == 0) {
                int directNeighbors = 0;
                directNeighbors += mansionGridIsHouse(grid, x + 1, y) ? 1 : 0;
                directNeighbors += mansionGridIsHouse(grid, x - 1, y) ? 1 : 0;
                directNeighbors += mansionGridIsHouse(grid, x, y + 1) ? 1 : 0;
                directNeighbors += mansionGridIsHouse(grid, x, y - 1) ? 1 : 0;
                if (directNeighbors >= 3) {
                    grid.set(x, y, 2);
                    touched = true;
                } else if (directNeighbors == 2) {
                    int diagonalNeighbors = 0;
                    diagonalNeighbors += mansionGridIsHouse(grid, x + 1, y + 1) ? 1 : 0;
                    diagonalNeighbors += mansionGridIsHouse(grid, x - 1, y + 1) ? 1 : 0;
                    diagonalNeighbors += mansionGridIsHouse(grid, x + 1, y - 1) ? 1 : 0;
                    diagonalNeighbors += mansionGridIsHouse(grid, x - 1, y - 1) ? 1 : 0;
                    if (diagonalNeighbors <= 1) {
                        grid.set(x, y, 2);
                        touched = true;
                    }
                }
            }
        }
    }

    return touched;
}

} // namespace mc::levelgen::structure::structures
