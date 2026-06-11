// 1:1 C++ port of the PURE, RNG-driven 2D mansion-layout generator nested in the
// REAL decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces
//     -> private static class MansionGrid          (WoodlandMansionPieces.java:101-402)
//
// MansionGrid takes ONE RandomSource and, in its constructor, deterministically
// builds five SimpleGrids:
//   baseGrid, thirdFloorGrid, floorRooms[0], floorRooms[1], floorRooms[2].
// It is completely self-contained PURE geometry + RNG:
//   * no world writes, no registry/datapack, no StructureTemplate.
//   * the ONLY external state is the RandomSource it is handed.
//   * leans on the already-certified SimpleGrid (SimpleGrid.h) for storage.
//
// Methods ported 1:1 (line refs into WoodlandMansionPieces.java):
//   MansionGrid(RandomSource)                                   :125-159 (ctor pipeline)
//   static boolean isHouse(SimpleGrid, x, y)                    :161-164
//   boolean isRoomId(SimpleGrid, x, y, floor, roomId)          :166-168
//   Direction get1x2RoomDirection(SimpleGrid, x, y, floor, id) :170-180
//   void recursiveCorridor(SimpleGrid, x, y, heading, depth)   :182-210  (RNG)
//   boolean cleanEdges(SimpleGrid)                              :212-242
//   void setupThirdFloor()                                     :244-300  (RNG)
//   void identifyRooms(SimpleGrid from, SimpleGrid room)       :302-401  (RNG, Util.shuffle)
//
// RNG ORDER is the whole point of this gate. The constructor draws from `random`
// in EXACTLY this sequence: recursiveCorridor x4 (baseGrid), then identifyRooms
// (baseGrid->floorRooms[0]) [Util.shuffle + per-room nextBoolean*2], then
// identifyRooms (baseGrid->floorRooms[1]), then setupThirdFloor()
// [nextInt(potentialRooms.size()), nextInt(potentialCorridors.size()),
// recursiveCorridor on thirdFloorGrid], then identifyRooms
// (thirdFloorGrid->floorRooms[2]). A single misordered/extra/missing draw shifts
// every later cell -> the gate catches it.
//
// 1:1 TRAPS this gate locks down:
//   - recursiveCorridor's 8-attempt loop: nextDir = Direction.from2DDataValue(nextInt(4));
//     the `(nextDir != EAST || !nextBoolean())` short-circuit only draws nextBoolean
//     when nextDir==EAST (Java || short-circuits) -> RNG draw count is data-dependent.
//   - Util.shuffle is Fisher-Yates from i=size down to i=2: swapTo=nextInt(i),
//     swap (i-1, swapTo). Drawn even when size<=1 draws nothing.
//   - identifyRooms draws nextBoolean()*2 (doorX,doorY) for EVERY freshly-claimed room.
//   - Direction.from2DDataValue(n) = BY_2D_DATA[abs(n%4)], BY_2D_DATA sorted by data2d:
//     {SOUTH(0), WEST(1), NORTH(2), EAST(3)}.
//   - Plane.HORIZONTAL iteration order is {NORTH, EAST, SOUTH, WEST}.
//   - bit flags are Java ints (CLEAR=0..BLOCKED=5, ROOM_1x1=65536, ROOM_1x2=131072,
//     ROOM_2x2=262144, ORIGIN=1048576, DOOR=2097152, STAIRS=4194304,
//     CORRIDOR=8388608, TYPE_MASK=983040, ID_MASK=65535) — verbatim.
//   - setupThirdFloor's "restore on dead-end" path rewrites floor cell back to roomData
//     (no RNG) but still consumed nextInt for room choice earlier.
//
// Indexing & RNG match Java exactly. SimpleGrid is row-major grid[x][y].

#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "world/level/levelgen/RandomSource.h"
// SimpleGrid (the certified 2D-grid helper) lives in WoodlandMansionGrid.h.
#include "world/level/levelgen/structure/structures/WoodlandMansionGrid.h"

namespace mc::levelgen::structure::structures {

class WoodlandMansionGridLayout {
public:
    // ---- Direction model (only the 4 horizontals are exercised) -------------
    // Java net.minecraft.core.Direction:
    //   NORTH(data2d=2, step=(0,0,-1)), SOUTH(0,(0,0,1)),
    //   WEST(1,(-1,0,0)),  EAST(3,(1,0,0)).
    enum class Dir { NORTH, SOUTH, WEST, EAST };

    static int stepX(Dir d) {
        switch (d) {
            case Dir::WEST: return -1;
            case Dir::EAST: return 1;
            default: return 0; // NORTH, SOUTH
        }
    }
    static int stepZ(Dir d) {
        switch (d) {
            case Dir::NORTH: return -1;
            case Dir::SOUTH: return 1;
            default: return 0; // WEST, EAST
        }
    }
    // Direction.getOpposite() — from3DDataValue(oppositeIndex); horizontals only.
    static Dir opposite(Dir d) {
        switch (d) {
            case Dir::NORTH: return Dir::SOUTH;
            case Dir::SOUTH: return Dir::NORTH;
            case Dir::WEST: return Dir::EAST;
            case Dir::EAST: return Dir::WEST;
        }
        return d;
    }
    // Direction.getClockWise() (Y axis) — :195-203.
    static Dir clockWise(Dir d) {
        switch (d) {
            case Dir::NORTH: return Dir::EAST;
            case Dir::SOUTH: return Dir::WEST;
            case Dir::WEST: return Dir::NORTH;
            case Dir::EAST: return Dir::SOUTH;
        }
        return d;
    }
    // Direction.getCounterClockWise() (Y axis) — :245-253.
    static Dir counterClockWise(Dir d) {
        switch (d) {
            case Dir::NORTH: return Dir::WEST;
            case Dir::SOUTH: return Dir::EAST;
            case Dir::WEST: return Dir::SOUTH;
            case Dir::EAST: return Dir::NORTH;
        }
        return d;
    }
    // Direction.from2DDataValue(n) = BY_2D_DATA[Mth.abs(n % 4)],
    // BY_2D_DATA sorted by data2d: {SOUTH(0), WEST(1), NORTH(2), EAST(3)}.
    static Dir from2DDataValue(int n) {
        // Java: Mth.abs(data % BY_2D_DATA.length) ; BY_2D_DATA.length == 4.
        int idx = javaIntAbs(n % 4);
        switch (idx) {
            case 0: return Dir::SOUTH;
            case 1: return Dir::WEST;
            case 2: return Dir::NORTH;
            default: return Dir::EAST; // 3
        }
    }
    // Direction.Plane.HORIZONTAL iteration order — :577.
    static const Dir* horizontalPlane() {
        static const Dir kHorizontal[4] = {Dir::NORTH, Dir::EAST, Dir::SOUTH, Dir::WEST};
        return kHorizontal;
    }

    // Math.abs(int): Math.abs(Integer.MIN_VALUE) == Integer.MIN_VALUE.
    static int javaIntAbs(int v) {
        return v < 0 ? static_cast<int>(0u - static_cast<uint32_t>(v)) : v;
    }

    // ---- MansionGrid flag constants (WoodlandMansionPieces.java:102-117) -----
    static constexpr int DEFAULT_SIZE = 11;
    static constexpr int CLEAR = 0;
    static constexpr int CORRIDOR = 1;
    static constexpr int ROOM = 2;
    static constexpr int START_ROOM = 3;
    static constexpr int TEST_ROOM = 4;
    static constexpr int BLOCKED = 5;
    static constexpr int ROOM_1x1 = 65536;
    static constexpr int ROOM_1x2 = 131072;
    static constexpr int ROOM_2x2 = 262144;
    static constexpr int ROOM_ORIGIN_FLAG = 1048576;
    static constexpr int ROOM_DOOR_FLAG = 2097152;
    static constexpr int ROOM_STAIRS_FLAG = 4194304;
    static constexpr int ROOM_CORRIDOR_FLAG = 8388608;
    static constexpr int ROOM_TYPE_MASK = 983040;
    static constexpr int ROOM_ID_MASK = 65535;

    SimpleGrid baseGrid;
    SimpleGrid thirdFloorGrid;
    SimpleGrid floorRooms[3];
    int entranceX = 0;
    int entranceY = 0;

    // MansionGrid(RandomSource) — :125-159.
    explicit WoodlandMansionGridLayout(RandomSource& random)
        : baseGrid(11, 11, 5),
          thirdFloorGrid(11, 11, 5),
          floorRooms{SimpleGrid(11, 11, 5), SimpleGrid(11, 11, 5), SimpleGrid(11, 11, 5)},
          random_(&random) {
        this->entranceX = 7;
        this->entranceY = 4;
        baseGrid.set(this->entranceX, this->entranceY, this->entranceX + 1, this->entranceY + 1, 3);
        baseGrid.set(this->entranceX - 1, this->entranceY, this->entranceX - 1, this->entranceY + 1, 2);
        baseGrid.set(this->entranceX + 2, this->entranceY - 2, this->entranceX + 3, this->entranceY + 3, 5);
        baseGrid.set(this->entranceX + 1, this->entranceY - 2, this->entranceX + 1, this->entranceY - 1, 1);
        baseGrid.set(this->entranceX + 1, this->entranceY + 2, this->entranceX + 1, this->entranceY + 3, 1);
        baseGrid.set(this->entranceX - 1, this->entranceY - 1, 1);
        baseGrid.set(this->entranceX - 1, this->entranceY + 2, 1);
        baseGrid.set(0, 0, 11, 1, 5);
        baseGrid.set(0, 9, 11, 11, 5);
        recursiveCorridor(baseGrid, this->entranceX, this->entranceY - 2, Dir::WEST, 6);
        recursiveCorridor(baseGrid, this->entranceX, this->entranceY + 3, Dir::WEST, 6);
        recursiveCorridor(baseGrid, this->entranceX - 2, this->entranceY - 1, Dir::WEST, 3);
        recursiveCorridor(baseGrid, this->entranceX - 2, this->entranceY + 2, Dir::WEST, 3);

        while (cleanEdges(baseGrid)) {
        }

        // floorRooms[0..2] already 11x11x5 via the member init above.
        identifyRooms(baseGrid, floorRooms[0]);
        identifyRooms(baseGrid, floorRooms[1]);
        floorRooms[0].set(this->entranceX + 1, this->entranceY, this->entranceX + 1, this->entranceY + 1, 8388608);
        floorRooms[1].set(this->entranceX + 1, this->entranceY, this->entranceX + 1, this->entranceY + 1, 8388608);
        // thirdFloorGrid already 11x11x5 (baseGrid.width()==baseGrid.height()==11).
        setupThirdFloor();
        identifyRooms(thirdFloorGrid, floorRooms[2]);
    }

    // isHouse — :161-164.
    static bool isHouse(const SimpleGrid& grid, int x, int y) {
        int value = grid.get(x, y);
        return value == 1 || value == 2 || value == 3 || value == 4;
    }

    // isRoomId — :166-168.
    bool isRoomId(const SimpleGrid& /*grid*/, int x, int y, int floor, int roomId) const {
        return (floorRooms[floor].get(x, y) & 65535) == roomId;
    }

    // get1x2RoomDirection — :170-180. Returns true into *out if a direction matched.
    bool get1x2RoomDirection(const SimpleGrid& grid, int x, int y, int floorNum, int roomId, Dir* out) const {
        const Dir* h = horizontalPlane();
        for (int i = 0; i < 4; i++) {
            Dir direction = h[i];
            if (isRoomId(grid, x + stepX(direction), y + stepZ(direction), floorNum, roomId)) {
                if (out) *out = direction;
                return true;
            }
        }
        return false;
    }

private:
    RandomSource* random_;

    // recursiveCorridor — :182-210.
    void recursiveCorridor(SimpleGrid& grid, int x, int y, Dir heading, int depth) {
        if (depth > 0) {
            grid.set(x, y, 1);
            grid.setif(x + stepX(heading), y + stepZ(heading), 0, 1);

            for (int attempts = 0; attempts < 8; attempts++) {
                Dir nextDir = from2DDataValue(random_->nextInt(4));
                // Java: nextDir != heading.getOpposite() && (nextDir != EAST || !nextBoolean())
                // The nextBoolean() draw only happens when nextDir == EAST (|| short-circuits),
                // AND only when the first conjunct (nextDir != opposite) is true (&& short-circuits).
                if (nextDir != opposite(heading) && (nextDir != Dir::EAST || !random_->nextBoolean())) {
                    int nx = x + stepX(heading);
                    int ny = y + stepZ(heading);
                    if (grid.get(nx + stepX(nextDir), ny + stepZ(nextDir)) == 0
                        && grid.get(nx + stepX(nextDir) * 2, ny + stepZ(nextDir) * 2) == 0) {
                        recursiveCorridor(grid, x + stepX(heading) + stepX(nextDir),
                                          y + stepZ(heading) + stepZ(nextDir), nextDir, depth - 1);
                        break;
                    }
                }
            }

            Dir cw = clockWise(heading);
            Dir ccw = counterClockWise(heading);
            grid.setif(x + stepX(cw), y + stepZ(cw), 0, 2);
            grid.setif(x + stepX(ccw), y + stepZ(ccw), 0, 2);
            grid.setif(x + stepX(heading) + stepX(cw), y + stepZ(heading) + stepZ(cw), 0, 2);
            grid.setif(x + stepX(heading) + stepX(ccw), y + stepZ(heading) + stepZ(ccw), 0, 2);
            grid.setif(x + stepX(heading) * 2, y + stepZ(heading) * 2, 0, 2);
            grid.setif(x + stepX(cw) * 2, y + stepZ(cw) * 2, 0, 2);
            grid.setif(x + stepX(ccw) * 2, y + stepZ(ccw) * 2, 0, 2);
        }
    }

    // cleanEdges — :212-242.
    bool cleanEdges(SimpleGrid& grid) {
        bool touched = false;

        for (int y = 0; y < grid.height(); y++) {
            for (int x = 0; x < grid.width(); x++) {
                if (grid.get(x, y) == 0) {
                    int directNeighbors = 0;
                    directNeighbors += isHouse(grid, x + 1, y) ? 1 : 0;
                    directNeighbors += isHouse(grid, x - 1, y) ? 1 : 0;
                    directNeighbors += isHouse(grid, x, y + 1) ? 1 : 0;
                    directNeighbors += isHouse(grid, x, y - 1) ? 1 : 0;
                    if (directNeighbors >= 3) {
                        grid.set(x, y, 2);
                        touched = true;
                    } else if (directNeighbors == 2) {
                        int diagonalNeighbors = 0;
                        diagonalNeighbors += isHouse(grid, x + 1, y + 1) ? 1 : 0;
                        diagonalNeighbors += isHouse(grid, x - 1, y + 1) ? 1 : 0;
                        diagonalNeighbors += isHouse(grid, x + 1, y - 1) ? 1 : 0;
                        diagonalNeighbors += isHouse(grid, x - 1, y - 1) ? 1 : 0;
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

    // setupThirdFloor — :244-300.
    void setupThirdFloor() {
        std::vector<std::pair<int, int>> potentialRooms;
        SimpleGrid& floor = floorRooms[1];

        for (int y = 0; y < thirdFloorGrid.height(); y++) {
            for (int x = 0; x < thirdFloorGrid.width(); x++) {
                int roomData = floor.get(x, y);
                int roomType = roomData & 983040;
                if (roomType == 131072 && (roomData & 2097152) == 2097152) {
                    potentialRooms.emplace_back(x, y);
                }
            }
        }

        if (potentialRooms.empty()) {
            thirdFloorGrid.set(0, 0, thirdFloorGrid.width(), thirdFloorGrid.height(), 5);
        } else {
            std::pair<int, int> roomPos = potentialRooms[random_->nextInt(static_cast<int>(potentialRooms.size()))];
            int roomData = floor.get(roomPos.first, roomPos.second);
            floor.set(roomPos.first, roomPos.second, roomData | 4194304);
            Dir roomDir;
            // get1x2RoomDirection: in vanilla a matching neighbour always exists for a
            // 1x2 room with the DOOR flag set, so the result is always present here.
            get1x2RoomDirection(baseGrid, roomPos.first, roomPos.second, 1, roomData & 65535, &roomDir);
            int roomEndX = roomPos.first + stepX(roomDir);
            int roomEndY = roomPos.second + stepZ(roomDir);

            for (int y = 0; y < thirdFloorGrid.height(); y++) {
                for (int x = 0; x < thirdFloorGrid.width(); x++) {
                    if (!isHouse(baseGrid, x, y)) {
                        thirdFloorGrid.set(x, y, 5);
                    } else if (x == roomPos.first && y == roomPos.second) {
                        thirdFloorGrid.set(x, y, 3);
                    } else if (x == roomEndX && y == roomEndY) {
                        thirdFloorGrid.set(x, y, 3);
                        floorRooms[2].set(x, y, 8388608);
                    }
                }
            }

            std::vector<Dir> potentialCorridors;
            const Dir* h = horizontalPlane();
            for (int i = 0; i < 4; i++) {
                Dir direction = h[i];
                if (thirdFloorGrid.get(roomEndX + stepX(direction), roomEndY + stepZ(direction)) == 0) {
                    potentialCorridors.push_back(direction);
                }
            }

            if (potentialCorridors.empty()) {
                thirdFloorGrid.set(0, 0, thirdFloorGrid.width(), thirdFloorGrid.height(), 5);
                floor.set(roomPos.first, roomPos.second, roomData);
            } else {
                Dir corridorDir = potentialCorridors[random_->nextInt(static_cast<int>(potentialCorridors.size()))];
                recursiveCorridor(thirdFloorGrid, roomEndX + stepX(corridorDir),
                                  roomEndY + stepZ(corridorDir), corridorDir, 4);

                while (cleanEdges(thirdFloorGrid)) {
                }
            }
        }
    }

    // Util.shuffle(List, RandomSource) — Util.java:1061-1068. Fisher-Yates.
    void utilShuffle(std::vector<std::pair<int, int>>& list) {
        int size = static_cast<int>(list.size());
        for (int i = size; i > 1; i--) {
            int swapTo = random_->nextInt(i);
            std::swap(list[i - 1], list[swapTo]);
        }
    }

    // identifyRooms — :302-401.
    void identifyRooms(const SimpleGrid& fromGrid, SimpleGrid& roomGrid) {
        std::vector<std::pair<int, int>> roomPos;

        for (int y = 0; y < fromGrid.height(); y++) {
            for (int x = 0; x < fromGrid.width(); x++) {
                if (fromGrid.get(x, y) == 2) {
                    roomPos.emplace_back(x, y);
                }
            }
        }

        utilShuffle(roomPos);
        int roomId = 10;

        for (const auto& pos : roomPos) {
            int x = pos.first;
            int y = pos.second;
            if (roomGrid.get(x, y) == 0) {
                int x0 = x;
                int x1 = x;
                int y0 = y;
                int y1 = y;
                int type = 65536;
                if (roomGrid.get(x + 1, y) == 0
                    && roomGrid.get(x, y + 1) == 0
                    && roomGrid.get(x + 1, y + 1) == 0
                    && fromGrid.get(x + 1, y) == 2
                    && fromGrid.get(x, y + 1) == 2
                    && fromGrid.get(x + 1, y + 1) == 2) {
                    x1++;
                    y1++;
                    type = 262144;
                } else if (roomGrid.get(x - 1, y) == 0
                           && roomGrid.get(x, y + 1) == 0
                           && roomGrid.get(x - 1, y + 1) == 0
                           && fromGrid.get(x - 1, y) == 2
                           && fromGrid.get(x, y + 1) == 2
                           && fromGrid.get(x - 1, y + 1) == 2) {
                    x0--;
                    y1++;
                    type = 262144;
                } else if (roomGrid.get(x - 1, y) == 0
                           && roomGrid.get(x, y - 1) == 0
                           && roomGrid.get(x - 1, y - 1) == 0
                           && fromGrid.get(x - 1, y) == 2
                           && fromGrid.get(x, y - 1) == 2
                           && fromGrid.get(x - 1, y - 1) == 2) {
                    x0--;
                    y0--;
                    type = 262144;
                } else if (roomGrid.get(x + 1, y) == 0 && fromGrid.get(x + 1, y) == 2) {
                    x1++;
                    type = 131072;
                } else if (roomGrid.get(x, y + 1) == 0 && fromGrid.get(x, y + 1) == 2) {
                    y1++;
                    type = 131072;
                } else if (roomGrid.get(x - 1, y) == 0 && fromGrid.get(x - 1, y) == 2) {
                    x0--;
                    type = 131072;
                } else if (roomGrid.get(x, y - 1) == 0 && fromGrid.get(x, y - 1) == 2) {
                    y0--;
                    type = 131072;
                }

                int doorX = random_->nextBoolean() ? x0 : x1;
                int doorY = random_->nextBoolean() ? y0 : y1;
                int doorFlag = 2097152;
                if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                    doorX = doorX == x0 ? x1 : x0;
                    doorY = doorY == y0 ? y1 : y0;
                    if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                        doorY = doorY == y0 ? y1 : y0;
                        if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                            doorX = doorX == x0 ? x1 : x0;
                            doorY = doorY == y0 ? y1 : y0;
                            if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                                doorFlag = 0;
                                doorX = x0;
                                doorY = y0;
                            }
                        }
                    }
                }

                for (int ry = y0; ry <= y1; ry++) {
                    for (int rx = x0; rx <= x1; rx++) {
                        if (rx == doorX && ry == doorY) {
                            roomGrid.set(rx, ry, 1048576 | doorFlag | type | roomId);
                        } else {
                            roomGrid.set(rx, ry, type | roomId);
                        }
                    }
                }

                roomId++;
            }
        }
    }
};

} // namespace mc::levelgen::structure::structures
