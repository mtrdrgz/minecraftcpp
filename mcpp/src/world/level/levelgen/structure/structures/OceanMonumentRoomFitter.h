#pragma once

// 1:1 port of the PURE room-type "fitter" predicates + their claim-marking nested
// in the REAL decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     -> private interface MonumentRoomFitter                       [line 721-725]
//          boolean fits(RoomDefinition)
//          OceanMonumentPiece create(Direction, RoomDefinition, RandomSource)
//     -> the seven implementing private static classes:
//          FitDoubleXYRoom   [49-73]   FitDoubleYZRoom   [91-115]
//          FitDoubleZRoom    [117-136] FitDoubleXRoom    [33-47]
//          FitDoubleYRoom    [75-89]   FitSimpleTopRoom  [153-170]
//          FitSimpleRoom     [138-151]
//
// MonumentBuilding's room-assignment loop (OceanMonumentPieces.java:198-206) walks
// the (shuffled) room list and, for each unclaimed non-special RoomDefinition, asks
// the seven fitters IN ORDER which one `fits`; the FIRST that does gets to `create`
// the sub-room and MARK the consumed RoomDefinition(s) `claimed`. Both halves are
// PURE: `fits` reads only `hasOpening[]` and the `claimed` flag of (up to two-hop)
// neighbours; `create`'s observable effect on the graph is the set of `claimed`
// flags it flips (the RandomSource it receives flows ONLY into the constructed
// OceanMonumentPiece's bounding box, never into which rooms are claimed). There are
// NO world writes, NO BlockState, NO registry/datapack, and NO RandomSource in the
// part ported here.
//
// The deterministic room graph (RoomDefinition / RoomGraph / Direction encoding,
// getRoomIndex, buildDeterministicRoomGraph) is reused verbatim from the sibling,
// already-byte-exact OceanMonumentRoomGraph.h (gated by
// ocean_monument_room_graph_parity).
//
// 1:1 TRAPS faithfully reproduced:
//   * EVERY array access is indexed by Direction.get3DDataValue()
//     (DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5) — exactly the Java path, never an
//     enum ordinal shortcut.
//   * The fitter PROBE ORDER in MonumentBuilding is NOT the declaration order of the
//     classes: it is XY, YZ, Z, X, Y, SimpleTop, Simple (OceanMonumentPieces.java
//     :190-196). fitFirst() iterates exactly that list and returns the first match.
//   * FitDoubleXRoom.fits / FitDoubleYRoom.fits are single-flag tests; FitDoubleZRoom
//     .fits tests the NORTH flag — but its create() has the quirk: if the room's own
//     NORTH opening is unusable (no NORTH opening OR the NORTH neighbour already
//     claimed) it instead claims the SOUTH neighbour as the `source` and that
//     source's NORTH connection (i.e. it pairs the cell to its south). We replicate
//     that source-swap precisely (OceanMonumentPieces.java:127-134).
//   * FitDoubleXYRoom.fits / FitDoubleYZRoom.fits are TWO-HOP: they require the
//     primary opening's neighbour to itself have a free UP opening, and create()
//     claims FOUR rooms (self, side, up, side.up) in that exact order.
//   * FitSimpleTopRoom.fits requires ALL of W/E/N/S/UP openings to be ABSENT (note:
//     it does NOT test the DOWN opening). FitSimpleRoom.fits is unconditionally true,
//     so it is the catch-all last fitter.
//   * create() dereferences connections[dir] WITHOUT a null check exactly like Java
//     (a NullPointerException there would be a real port bug surfaced by the gate —
//     fits() is the guard that guarantees the neighbour exists for the matched
//     fitter). We mirror Java: the matched fitter's create only ever touches links
//     it has just proven present.
//
// Certified byte-exact by ocean_monument_room_fitter_parity
// (tools/OceanMonumentRoomFitterParity.java drives the REAL MonumentRoomFitter
// implementations via reflection on identically-built graphs and emits a TSV; this
// header recomputes and compares).

#include <array>
#include <cstdint>

#include "world/level/levelgen/structure/structures/OceanMonumentRoomGraph.h"

namespace mc::levelgen::structure::oceanmonumentfitter {

using oceanmonumentgraph::Direction;
using oceanmonumentgraph::RoomGraph;
using oceanmonumentgraph::RoomDefinition;
using oceanmonumentgraph::get3DDataValue;
using oceanmonumentgraph::oppositeIndex;

// The seven fitter kinds, in the MonumentBuilding PROBE ORDER (NOT class
// declaration order). FitFirst() walks this enum's value order.
//   OceanMonumentPieces.java:190-196.
enum class FitterKind : int32_t {
    DoubleXY = 0,   // FitDoubleXYRoom
    DoubleYZ = 1,   // FitDoubleYZRoom
    DoubleZ  = 2,   // FitDoubleZRoom
    DoubleX  = 3,   // FitDoubleXRoom
    DoubleY  = 4,   // FitDoubleYRoom
    SimpleTop = 5,  // FitSimpleTopRoom
    Simple   = 6    // FitSimpleRoom
};

// Probe order used by MonumentBuilding's assignment loop.
constexpr std::array<FitterKind, 7> PROBE_ORDER = {
    FitterKind::DoubleXY, FitterKind::DoubleYZ, FitterKind::DoubleZ,
    FitterKind::DoubleX,  FitterKind::DoubleY,  FitterKind::SimpleTop,
    FitterKind::Simple};

// 3D-data indices of the directions the fitters reference, computed exactly the way
// the Java does (Direction.<DIR>.get3DDataValue()).
constexpr int DI_UP    = get3DDataValue(Direction::UP);
constexpr int DI_NORTH = get3DDataValue(Direction::NORTH);
constexpr int DI_SOUTH = get3DDataValue(Direction::SOUTH);
constexpr int DI_WEST  = get3DDataValue(Direction::WEST);
constexpr int DI_EAST  = get3DDataValue(Direction::EAST);

// ── fits(RoomDefinition) for each fitter (OceanMonumentPieces.java) ──────────────
// Every predicate reads only the room's hasOpening[] and the `claimed` flag of the
// (possibly two-hop) neighbour it would consume. `def` is a node id into g.rooms.

// FitDoubleXRoom.fits — :35-37.
//   hasOpening[EAST] && !connections[EAST].claimed
inline bool fitsDoubleX(const RoomGraph& g, int def) {
    const RoomDefinition& d = g.rooms[def];
    return d.hasOpening[DI_EAST] && !g.rooms[d.connections[DI_EAST]].claimed;
}

// FitDoubleYRoom.fits — :77-79.
//   hasOpening[UP] && !connections[UP].claimed
inline bool fitsDoubleY(const RoomGraph& g, int def) {
    const RoomDefinition& d = g.rooms[def];
    return d.hasOpening[DI_UP] && !g.rooms[d.connections[DI_UP]].claimed;
}

// FitDoubleZRoom.fits — :119-121.
//   hasOpening[NORTH] && !connections[NORTH].claimed
inline bool fitsDoubleZ(const RoomGraph& g, int def) {
    const RoomDefinition& d = g.rooms[def];
    return d.hasOpening[DI_NORTH] && !g.rooms[d.connections[DI_NORTH]].claimed;
}

// FitDoubleXYRoom.fits — :51-60. Two-hop: self has free EAST+UP openings AND the
// EAST neighbour itself has a free UP opening.
inline bool fitsDoubleXY(const RoomGraph& g, int def) {
    const RoomDefinition& d = g.rooms[def];
    if (d.hasOpening[DI_EAST] && !g.rooms[d.connections[DI_EAST]].claimed
        && d.hasOpening[DI_UP] && !g.rooms[d.connections[DI_UP]].claimed) {
        const RoomDefinition& east = g.rooms[d.connections[DI_EAST]];
        return east.hasOpening[DI_UP] && !g.rooms[east.connections[DI_UP]].claimed;
    }
    return false;
}

// FitDoubleYZRoom.fits — :93-102. Two-hop: self has free NORTH+UP openings AND the
// NORTH neighbour itself has a free UP opening.
inline bool fitsDoubleYZ(const RoomGraph& g, int def) {
    const RoomDefinition& d = g.rooms[def];
    if (d.hasOpening[DI_NORTH] && !g.rooms[d.connections[DI_NORTH]].claimed
        && d.hasOpening[DI_UP] && !g.rooms[d.connections[DI_UP]].claimed) {
        const RoomDefinition& north = g.rooms[d.connections[DI_NORTH]];
        return north.hasOpening[DI_UP] && !g.rooms[north.connections[DI_UP]].claimed;
    }
    return false;
}

// FitSimpleTopRoom.fits — :155-160. ALL of W/E/N/S/UP openings absent (DOWN ignored).
inline bool fitsSimpleTop(const RoomGraph& g, int def) {
    const RoomDefinition& d = g.rooms[def];
    return !d.hasOpening[DI_WEST] && !d.hasOpening[DI_EAST]
        && !d.hasOpening[DI_NORTH] && !d.hasOpening[DI_SOUTH]
        && !d.hasOpening[DI_UP];
}

// FitSimpleRoom.fits — :140-142. Always true (catch-all).
inline bool fitsSimple(const RoomGraph& /*g*/, int /*def*/) { return true; }

// Dispatch fits() by kind.
inline bool fits(FitterKind kind, const RoomGraph& g, int def) {
    switch (kind) {
        case FitterKind::DoubleXY:  return fitsDoubleXY(g, def);
        case FitterKind::DoubleYZ:  return fitsDoubleYZ(g, def);
        case FitterKind::DoubleZ:   return fitsDoubleZ(g, def);
        case FitterKind::DoubleX:   return fitsDoubleX(g, def);
        case FitterKind::DoubleY:   return fitsDoubleY(g, def);
        case FitterKind::SimpleTop: return fitsSimpleTop(g, def);
        case FitterKind::Simple:    return fitsSimple(g, def);
    }
    return false;
}

// MonumentBuilding assignment probe: return the FIRST fitter (in PROBE_ORDER) whose
// fits() is true, or one-past-end if none. (FitSimpleRoom is always-true, so a
// non-special unclaimed room always matches something.) — :198-206.
inline FitterKind fitFirst(const RoomGraph& g, int def) {
    for (FitterKind k : PROBE_ORDER)
        if (fits(k, g, def)) return k;
    return FitterKind::Simple; // unreachable: FitSimpleRoom always matches
}

// ── create(...) claim-marking for each fitter (mutates g; mirrors Java) ──────────
// The RandomSource arg is dropped: it influences only the constructed piece's box,
// never the `claimed` flags. Each helper flips EXACTLY the flags the real create()
// does, IN THE SAME ORDER, so a snapshot of g after the call is byte-comparable.

// FitDoubleXRoom.create — :42-45.
inline void createDoubleX(RoomGraph& g, int def) {
    g.rooms[def].claimed = true;
    g.rooms[g.rooms[def].connections[DI_EAST]].claimed = true;
}

// FitDoubleYRoom.create — :84-87.
inline void createDoubleY(RoomGraph& g, int def) {
    g.rooms[def].claimed = true;
    g.rooms[g.rooms[def].connections[DI_UP]].claimed = true;
}

// FitDoubleZRoom.create — :127-134. SOURCE-SWAP quirk: if this room's NORTH opening
// is unusable (absent OR its NORTH neighbour already claimed), the SOUTH neighbour
// becomes the `source` instead; then claim source + source.NORTH.
inline void createDoubleZ(RoomGraph& g, int def) {
    int source = def;
    if (!g.rooms[def].hasOpening[DI_NORTH]
        || g.rooms[g.rooms[def].connections[DI_NORTH]].claimed) {
        source = g.rooms[def].connections[DI_SOUTH];
    }
    g.rooms[source].claimed = true;
    g.rooms[g.rooms[source].connections[DI_NORTH]].claimed = true;
}

// FitDoubleXYRoom.create — :67-71. Claims self, EAST, UP, EAST.UP in that order.
inline void createDoubleXY(RoomGraph& g, int def) {
    g.rooms[def].claimed = true;
    int eastId = g.rooms[def].connections[DI_EAST];
    g.rooms[eastId].claimed = true;
    g.rooms[g.rooms[def].connections[DI_UP]].claimed = true;
    g.rooms[g.rooms[eastId].connections[DI_UP]].claimed = true;
}

// FitDoubleYZRoom.create — :109-113. Claims self, NORTH, UP, NORTH.UP in that order.
inline void createDoubleYZ(RoomGraph& g, int def) {
    g.rooms[def].claimed = true;
    int northId = g.rooms[def].connections[DI_NORTH];
    g.rooms[northId].claimed = true;
    g.rooms[g.rooms[def].connections[DI_UP]].claimed = true;
    g.rooms[g.rooms[northId].connections[DI_UP]].claimed = true;
}

// FitSimpleTopRoom.create — :167-168. Claims self only.
inline void createSimpleTop(RoomGraph& g, int def) { g.rooms[def].claimed = true; }

// FitSimpleRoom.create — :148-149. Claims self only.
inline void createSimple(RoomGraph& g, int def) { g.rooms[def].claimed = true; }

// Dispatch create() by kind.
inline void create(FitterKind kind, RoomGraph& g, int def) {
    switch (kind) {
        case FitterKind::DoubleXY:  createDoubleXY(g, def);  break;
        case FitterKind::DoubleYZ:  createDoubleYZ(g, def);  break;
        case FitterKind::DoubleZ:   createDoubleZ(g, def);   break;
        case FitterKind::DoubleX:   createDoubleX(g, def);   break;
        case FitterKind::DoubleY:   createDoubleY(g, def);   break;
        case FitterKind::SimpleTop: createSimpleTop(g, def); break;
        case FitterKind::Simple:    createSimple(g, def);    break;
    }
}

} // namespace mc::levelgen::structure::oceanmonumentfitter
