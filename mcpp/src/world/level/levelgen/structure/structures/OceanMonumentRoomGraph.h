#pragma once

// 1:1 port of the PURE room-connectivity graph nested in the REAL decompiled
// 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     -> private static class RoomDefinition        [line 1933-1987]
//          RoomDefinition(int roomIndex)
//          void    setConnection(Direction, RoomDefinition)   [1945-1948]
//          void    updateOpenings()                           [1950-1954]
//          boolean findSource(int scanIndex)                  [1956-1970]
//          boolean isSpecial()                                [1972-1974]
//          int     countOpenings()                            [1976-1986]
//     plus the DETERMINISTIC prefix of
//          MonumentBuilding.generateRoomGraph(RandomSource)   [228-309]
//        that wires the 5x3x5 room grid + 3 special wing/roof rooms together.
//
// This is a self-contained integer/boolean graph: NO world writes, NO BlockState,
// NO registry/datapack, and (in the part ported here) NO RandomSource. The grid
// CONSTRUCTION and connectivity is fully deterministic --- the only randomness in
// the real generateRoomGraph is (a) which core room is picked, (b) Util.shuffle,
// and (c) which openings the prune loop tries to close. We replicate the
// deterministic graph build, then exercise the graph helpers by SCRIPTING the
// open/close operations explicitly (no RNG), so the gate stays pure.
//
// 1:1 TRAPS faithfully reproduced:
//   * connections[]/hasOpening[] are indexed by Direction.get3DDataValue()
//     (DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5) --- NOT enum ordinal (here they
//     coincide, but the indexing path is via get3DDataValue exactly as Java).
//   * setConnection wires BOTH ends: this.connections[dir] and
//     definition.connections[dir.getOpposite()] (opposite via oppositeIndex table).
//   * generateRoomGraph iterates Direction.values() in DECLARATION order and uses
//     getStepX/Y/Z (= the Vec3i normal components) for neighbour offsets, and the
//     `neighZ == z ? dir : dir.getOpposite()` quirk when wiring.
//   * findSource is a recursive DFS with per-node scanIndex memoization and an
//     early-out on isSource; the C++ matches the exact short-circuit/visit order.
//   * isSpecial() == index >= 75 (the 1001/1002/1003 wing/roof rooms).
//
// Certified byte-exact by ocean_monument_room_graph_parity
// (tools/OceanMonumentRoomGraphParity.java drives the REAL RoomDefinition via
// reflection, applies the identical scripted ops, and emits a TSV; this header
// recomputes and compares).

#include <array>
#include <cstdint>
#include <vector>

namespace mc::levelgen::structure::oceanmonumentgraph {

// ------ net.minecraft.core.Direction (Direction.java:33-38) ------------------------------------------------------------------
// Each value carries its 3D data value and opposite 3D-data index, exactly as the
// enum constructor args (data3d, oppositeIndex).  Enum declaration order:
//   DOWN, UP, NORTH, SOUTH, WEST, EAST.
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// data3d table --- Direction.get3DDataValue() (== the first ctor arg). For this
// enum the 3D data value equals the ordinal, but we keep the lookup explicit so
// the indexing path mirrors Java's `direction.get3DDataValue()`.
constexpr int get3DDataValue(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN:  return 0;
        case Direction::UP:    return 1;
        case Direction::NORTH: return 2;
        case Direction::SOUTH: return 3;
        case Direction::WEST:  return 4;
        case Direction::EAST:  return 5;
    }
    return 0;
}

// Direction.getOpposite() --- returns from3DDataValue(oppositeIndex). The opposite
// 3D-data indices from the ctor: DOWN<->UP, NORTH<->SOUTH, WEST<->EAST.
constexpr int oppositeIndex(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN:  return 1;  // -> UP
        case Direction::UP:    return 0;  // -> DOWN
        case Direction::NORTH: return 3;  // -> SOUTH
        case Direction::SOUTH: return 2;  // -> NORTH
        case Direction::WEST:  return 5;  // -> EAST
        case Direction::EAST:  return 4;  // -> WEST
    }
    return 0;
}

// Direction.getStepX/Y/Z() --- the Vec3i `normal` components (Direction.java:33-38).
constexpr int getStepX(Direction d) noexcept {
    switch (d) { case Direction::WEST: return -1; case Direction::EAST: return 1; default: return 0; }
}
constexpr int getStepY(Direction d) noexcept {
    switch (d) { case Direction::DOWN: return -1; case Direction::UP: return 1; default: return 0; }
}
constexpr int getStepZ(Direction d) noexcept {
    switch (d) { case Direction::NORTH: return -1; case Direction::SOUTH: return 1; default: return 0; }
}

// Direction.values() in declaration order --- iterated by generateRoomGraph.
constexpr std::array<Direction, 6> DIRECTION_VALUES = {
    Direction::DOWN, Direction::UP, Direction::NORTH, Direction::SOUTH, Direction::WEST, Direction::EAST};

// OceanMonumentPiece.getRoomIndex(roomX,roomY,roomZ) --- OceanMonumentPieces.java
// :1450-1452:  roomY * 25 + roomZ * 5 + roomX.  (Battery stays in [0,74], no wrap.)
constexpr int getRoomIndex(int roomX, int roomY, int roomZ) noexcept {
    return roomY * 25 + roomZ * 5 + roomX;
}

// generateRoomGraph special-room indices, derived exactly like the real
// MonumentBuilding constants (OceanMonumentPieces.java:1441-1447, 282-287).
constexpr int GRIDROOM_SOURCE_INDEX        = getRoomIndex(2, 0, 0);  // == 2
constexpr int GRIDROOM_TOP_CONNECT_INDEX   = getRoomIndex(2, 2, 0);  // == 52
constexpr int GRIDROOM_LEFTWING_CONNECT_INDEX  = getRoomIndex(0, 1, 0);  // == 25
constexpr int GRIDROOM_RIGHTWING_CONNECT_INDEX = getRoomIndex(4, 1, 0);  // == 29
constexpr int LEFTWING_INDEX  = 1001;
constexpr int RIGHTWING_INDEX = 1002;
constexpr int PENTHOUSE_INDEX = 1003;

// ------ net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//      .RoomDefinition (OceanMonumentPieces.java:1933-1987) ------------------------------------------------------------
// The connections[] are pointers into a stable backing store (the real Java uses
// object references). We model rooms in a std::vector<RoomDefinition> and store
// connections as indices into that vector (-1 == null), so references stay valid.
struct RoomDefinition {
    int index = 0;
    std::array<int, 6> connections{};   // index into owning RoomGraph::rooms, or -1
    std::array<bool, 6> hasOpening{};
    bool claimed = false;
    bool isSource = false;
    int scanIndex = 0;

    RoomDefinition() { connections.fill(-1); }
    explicit RoomDefinition(int roomIndex) : index(roomIndex) { connections.fill(-1); }

    // RoomDefinition.isSpecial() --- OceanMonumentPieces.java:1972-1974.
    bool isSpecial() const noexcept { return index >= 75; }

    // RoomDefinition.countOpenings() --- OceanMonumentPieces.java:1976-1986.
    int countOpenings() const noexcept {
        int c = 0;
        for (int i = 0; i < 6; ++i)
            if (hasOpening[i]) ++c;
        return c;
    }

    // RoomDefinition.updateOpenings() --- OceanMonumentPieces.java:1950-1954.
    void updateOpenings() noexcept {
        for (int i = 0; i < 6; ++i)
            hasOpening[i] = connections[i] != -1;
    }
};

// Owns all RoomDefinition nodes and provides setConnection / findSource, which
// need to follow connection references. `rooms[i]` is a node; connections store
// node indices. Slot `grid[k]` (k in 0..74) holds the node index for grid cell k
// (or -1 if that cell has no room), matching the real `roomGrid[]` array.
struct RoomGraph {
    std::vector<RoomDefinition> rooms;
    std::array<int, 75> grid{};   // grid cell -> node index, or -1

    RoomGraph() { grid.fill(-1); }

    int addRoom(int index) {
        int id = static_cast<int>(rooms.size());
        rooms.emplace_back(index);
        return id;
    }

    // RoomDefinition.setConnection(Direction, RoomDefinition) --- :1945-1948.
    //   this.connections[direction.get3DDataValue()] = definition;
    //   definition.connections[direction.getOpposite().get3DDataValue()] = this;
    void setConnection(int selfId, Direction direction, int defId) noexcept {
        rooms[selfId].connections[get3DDataValue(direction)] = defId;
        rooms[defId].connections[oppositeIndex(direction)] = selfId;
    }

    // RoomDefinition.findSource(int scanIndex) --- :1956-1970 (recursive DFS with
    // per-node scanIndex memoization; visits connections in 3D-data index order).
    bool findSource(int nodeId, int scan) noexcept {
        RoomDefinition& self = rooms[nodeId];
        if (self.isSource) return true;
        self.scanIndex = scan;
        for (int i = 0; i < 6; ++i) {
            int c = self.connections[i];
            if (c != -1 && self.hasOpening[i] && rooms[c].scanIndex != scan && findSource(c, scan))
                return true;
        }
        return false;
    }
};

// Build the DETERMINISTIC room graph exactly as MonumentBuilding.generateRoomGraph
// does BEFORE any RandomSource use (OceanMonumentPieces.java:228-310). Returns a
// RoomGraph whose `grid[]` / `rooms[]` mirror the real `roomGrid[]`, with the
// source flag set and updateOpenings() applied to every room (and the roof room),
// just like the real code up to line 311 (the Util.shuffle / prune loop is left to
// the caller's scripted ops). The roof/leftWing/rightWing special rooms are the
// last three appended nodes; their grid index is N/A (special), accessible via the
// returned ids.
struct BuiltGraph {
    RoomGraph g;
    int roofRoomId = -1;
    int leftWingId = -1;
    int rightWingId = -1;
    int sourceRoomId = -1;
};

inline BuiltGraph buildDeterministicRoomGraph() {
    BuiltGraph out;
    RoomGraph& g = out.g;

    // y=0 layer: 5x4 grid (x 0..4, z 0..3)
    for (int x = 0; x < 5; ++x)
        for (int z = 0; z < 4; ++z) {
            int pos = getRoomIndex(x, 0, z);
            g.grid[pos] = g.addRoom(pos);
        }
    // y=1 layer: 5x4
    for (int x = 0; x < 5; ++x)
        for (int z = 0; z < 4; ++z) {
            int pos = getRoomIndex(x, 1, z);
            g.grid[pos] = g.addRoom(pos);
        }
    // y=2 layer: 3x2 (x 1..3, z 0..1)
    for (int x = 1; x < 4; ++x)
        for (int z = 0; z < 2; ++z) {
            int pos = getRoomIndex(x, 2, z);
            g.grid[pos] = g.addRoom(pos);
        }

    out.sourceRoomId = g.grid[GRIDROOM_SOURCE_INDEX];

    // Wire neighbours. Iterate cells (x,z,y) then Direction.values() --- matching the
    // real loop nesting (x outer, z mid, y inner) and enum-order direction walk.
    for (int x = 0; x < 5; ++x)
        for (int z = 0; z < 5; ++z)
            for (int y = 0; y < 3; ++y) {
                int pos = getRoomIndex(x, y, z);
                if (g.grid[pos] == -1) continue;
                for (Direction direction : DIRECTION_VALUES) {
                    int neighX = x + getStepX(direction);
                    int neighY = y + getStepY(direction);
                    int neighZ = z + getStepZ(direction);
                    if (neighX >= 0 && neighX < 5 && neighZ >= 0 && neighZ < 5 && neighY >= 0 && neighY < 3) {
                        int neighPos = getRoomIndex(neighX, neighY, neighZ);
                        if (g.grid[neighPos] != -1) {
                            if (neighZ == z)
                                g.setConnection(g.grid[pos], direction, g.grid[neighPos]);
                            else
                                g.setConnection(g.grid[pos],
                                                static_cast<Direction>(oppositeIndex(direction)),
                                                g.grid[neighPos]);
                        }
                    }
                }
            }

    // Special wing/roof rooms (OceanMonumentPieces.java:282-290).
    out.roofRoomId  = g.addRoom(PENTHOUSE_INDEX); // 1003
    out.leftWingId  = g.addRoom(LEFTWING_INDEX);  // 1001
    out.rightWingId = g.addRoom(RIGHTWING_INDEX); // 1002
    g.setConnection(g.grid[GRIDROOM_TOP_CONNECT_INDEX], Direction::UP, out.roofRoomId);
    g.setConnection(g.grid[GRIDROOM_LEFTWING_CONNECT_INDEX], Direction::SOUTH, out.leftWingId);
    g.setConnection(g.grid[GRIDROOM_RIGHTWING_CONNECT_INDEX], Direction::SOUTH, out.rightWingId);
    g.rooms[out.roofRoomId].claimed = true;
    g.rooms[out.leftWingId].claimed = true;
    g.rooms[out.rightWingId].claimed = true;
    g.rooms[out.sourceRoomId].isSource = true;

    // updateOpenings() on every grid room, then the roof room (matches :303-310).
    for (RoomDefinition& r : g.rooms) {
        // Only the grid rooms + roof are updated in the real loop; the left/right
        // wings are NOT updateOpenings()'d (they keep all-false hasOpening). We
        // emulate this by updating everything EXCEPT the two wings, then forcing
        // the wings back to all-false below.
        r.updateOpenings();
    }
    // Real code: only grid rooms (from roomGrid[]) + roofRoom get updateOpenings();
    // leftWing/rightWing do NOT. Reset their openings to the post-ctor state.
    for (int i = 0; i < 6; ++i) {
        g.rooms[out.leftWingId].hasOpening[i] = false;
        g.rooms[out.rightWingId].hasOpening[i] = false;
    }
    return out;
}

} // namespace mc::levelgen::structure::oceanmonumentgraph
