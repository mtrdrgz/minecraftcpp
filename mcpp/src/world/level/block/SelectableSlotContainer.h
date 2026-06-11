#pragma once
#include <cstdint>
#include <cmath>

// ---------------------------------------------------------------------------
// 1:1 C++ port of the pure slot-picking geometry of
//   net.minecraft.world.level.block.SelectableSlotContainer
//   (Minecraft Java Edition 26.1.2)
//
//   SelectableSlotContainer.getHitSlot(BlockHitResult, Direction)  :17-23
//   SelectableSlotContainer.getRelativeHitCoordinatesForBlockFace  :25-44
//   SelectableSlotContainer.getSection(float, int)                 :46-50
//
// This is the math that turns WHERE a player's crosshair struck a chiseled
// bookshelf (or any selectable-slot block) into a 0-based slot index. Given the
// hit Direction, the struck BlockPos, the precise hit Vec3 location, the block's
// FACING, and the grid size (rows x columns), it produces the slot under the
// crosshair — or "no slot" when the struck face is not the block's front face.
//
// It is a pure deterministic helper: it reads ONLY a Direction, three ints
// (block pos), three doubles (hit location), the block facing, and the grid
// dimensions. NO world/BlockGetter/registry/GL/entity-state coupling, so it
// ports standalone and is exercised against the REAL interface (default +
// private static methods, via reflection) by SelectableSlotContainerParity.java.
//
// Java source:
//   default OptionalInt getHitSlot(BlockHitResult hitResult, Direction blockFacing) {
//      return getRelativeHitCoordinatesForBlockFace(hitResult, blockFacing).map(hitCoords -> {
//         int row = getSection(1.0F - hitCoords.y, this.getRows());
//         int column = getSection(hitCoords.x, this.getColumns());
//         return OptionalInt.of(column + row * this.getColumns());
//      }).orElseGet(OptionalInt::empty);
//   }
//
//   private static Optional<Vec2> getRelativeHitCoordinatesForBlockFace(
//         BlockHitResult hitResult, Direction blockFacing) {
//      Direction hitDirection = hitResult.getDirection();
//      if (blockFacing != hitDirection) return Optional.empty();
//      BlockPos hitBlockPos = hitResult.getBlockPos().relative(hitDirection);
//      Vec3 relativeHit = hitResult.getLocation()
//            .subtract(hitBlockPos.getX(), hitBlockPos.getY(), hitBlockPos.getZ());
//      double relativeX = relativeHit.x();
//      double relativeY = relativeHit.y();
//      double relativeZ = relativeHit.z();
//      return switch (hitDirection) {
//         case NORTH -> Optional.of(new Vec2((float)(1.0 - relativeX), (float)relativeY));
//         case SOUTH -> Optional.of(new Vec2((float)relativeX, (float)relativeY));
//         case WEST  -> Optional.of(new Vec2((float)relativeZ, (float)relativeY));
//         case EAST  -> Optional.of(new Vec2((float)(1.0 - relativeZ), (float)relativeY));
//         case DOWN, UP -> Optional.empty();
//      };
//   }
//
//   private static int getSection(float relativeCoordinate, int maxSections) {
//      float targetedPixel = relativeCoordinate * 16.0F;
//      float sectionSize = 16.0F / maxSections;
//      return Mth.clamp(Mth.floor(targetedPixel / sectionSize), 0, maxSections - 1);
//   }
//
// 1:1 TRAPS reproduced exactly:
//   * BlockPos.relative(dir) adds the dir's INTEGER unit normal (getStepX/Y/Z) to
//     the block pos: the section/face math is computed in the cell ONE STEP OUT
//     along the hit face, exactly as Java does. We subtract those ints.
//   * relativeHit.subtract(int,int,int) widens the ints to double and subtracts
//     in DOUBLE; relativeX/Y/Z are doubles.
//   * The Vec2 components are `(float)(...)` — DOUBLE computed first
//     (e.g. 1.0 - relativeX in double), THEN narrowed to float (round-to-nearest-
//     even). Computing 1.0F - (float)relativeX would drift.
//   * getSection: `relativeCoordinate * 16.0F` and `16.0F / maxSections` are
//     FLOAT operations; `targetedPixel / sectionSize` is FLOAT division. Its
//     result (a float) is then passed to Mth.floor(float), which WIDENS the float
//     to double and applies (int)Math.floor — NOT a float floor. Doing the divide
//     in double would change rounding at section boundaries.
//   * `1.0F - hitCoords.y` in getHitSlot is FLOAT subtraction on the stored float.
//   * Mth.floor(float v) == (int)Math.floor((double)v) : narrowing the
//     Math.floor(double) result back to int (saturates for huge magnitudes, but
//     all finite inputs here stay well in range).
//   * Mth.clamp(int,min,max) == min(max(v,min),max).
//   * blockFacing != hitDirection short-circuits to "no slot" (empty) — and DOWN
//     / UP hit faces also return empty even when they equal blockFacing.
// ---------------------------------------------------------------------------

namespace mc::block_selectableslot {

// Java: net.minecraft.core.Direction ordinals (Direction.java:33-38).
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// Java: Direction.getStepX/Y/Z() — the Vec3i unit normal per constant
// (Direction.java:33-38). DOWN(0,-1,0) UP(0,1,0) NORTH(0,0,-1) SOUTH(0,0,1)
// WEST(-1,0,0) EAST(1,0,0).
inline constexpr int DIRECTION_STEP[6][3] = {
    {0, -1, 0}, // DOWN
    {0, 1, 0},  // UP
    {0, 0, -1}, // NORTH
    {0, 0, 1},  // SOUTH
    {-1, 0, 0}, // WEST
    {1, 0, 0},  // EAST
};

// Java: Mth.floor(float v) = (int)Math.floor(v) — the float widens to double
// (Mth.java:61-63).
inline int mthFloorF(float v) {
    return static_cast<int>(std::floor(static_cast<double>(v)));
}

// Java: Mth.clamp(int value,int min,int max) (Mth.java) = min(max(value,min),max).
inline int mthClampI(int value, int min, int max) {
    if (value < min) value = min;
    if (value > max) value = max;
    return value;
}

// Java: SelectableSlotContainer.getSection(float, int) — :46-50.
inline int getSection(float relativeCoordinate, int maxSections) {
    float targetedPixel = relativeCoordinate * 16.0F;            // float mul
    float sectionSize = 16.0F / static_cast<float>(maxSections); // float div
    return mthClampI(mthFloorF(targetedPixel / sectionSize), 0, maxSections - 1);
}

// Result of getRelativeHitCoordinatesForBlockFace: present? + the Vec2 (x,y).
struct RelativeHit {
    bool present;
    float x;
    float y;
};

// Java: SelectableSlotContainer.getRelativeHitCoordinatesForBlockFace(
//          BlockHitResult, Direction) — :25-44.
// hitDirection = hitResult.getDirection(); the struck BlockPos is (bx,by,bz);
// the precise hit location is (locX,locY,locZ) doubles.
inline RelativeHit getRelativeHitCoordinatesForBlockFace(
    Direction hitDirection,
    int bx, int by, int bz,
    double locX, double locY, double locZ,
    Direction blockFacing) {
    if (blockFacing != hitDirection) {
        return {false, 0.0F, 0.0F};
    }

    // hitBlockPos = blockPos.relative(hitDirection): integer step.
    const int* step = DIRECTION_STEP[static_cast<int>(hitDirection)];
    int hbx = bx + step[0];
    int hby = by + step[1];
    int hbz = bz + step[2];

    // relativeHit = location.subtract(hbx,hby,hbz): ints widen to double.
    double relativeX = locX - static_cast<double>(hbx);
    double relativeY = locY - static_cast<double>(hby);
    double relativeZ = locZ - static_cast<double>(hbz);

    switch (hitDirection) {
        case Direction::NORTH:
            return {true, static_cast<float>(1.0 - relativeX), static_cast<float>(relativeY)};
        case Direction::SOUTH:
            return {true, static_cast<float>(relativeX), static_cast<float>(relativeY)};
        case Direction::WEST:
            return {true, static_cast<float>(relativeZ), static_cast<float>(relativeY)};
        case Direction::EAST:
            return {true, static_cast<float>(1.0 - relativeZ), static_cast<float>(relativeY)};
        case Direction::DOWN:
        case Direction::UP:
        default:
            return {false, 0.0F, 0.0F};
    }
}

// Result of getHitSlot: present? + the slot index.
struct HitSlot {
    bool present;
    int slot;
};

// Java: SelectableSlotContainer.getHitSlot(BlockHitResult, Direction) — :17-23.
// rows/columns are the implementor's getRows()/getColumns() (e.g. ChiseledBookShelf
// = 2 rows x 3 columns).
inline HitSlot getHitSlot(
    Direction hitDirection,
    int bx, int by, int bz,
    double locX, double locY, double locZ,
    Direction blockFacing,
    int rows, int columns) {
    RelativeHit hit = getRelativeHitCoordinatesForBlockFace(
        hitDirection, bx, by, bz, locX, locY, locZ, blockFacing);
    if (!hit.present) {
        return {false, 0};
    }
    int row = getSection(1.0F - hit.y, rows);   // float subtraction on stored float
    int column = getSection(hit.x, columns);
    return {true, column + row * columns};
}

} // namespace mc::block_selectableslot
