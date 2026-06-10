// 1:1 port of net.minecraft.world.level.block.state.properties.RotationSegment
// (26.1.2). A thin static wrapper over a SegmentedAnglePrecision(4) — the 16-segment
// (4-bit binary angle) quantization used by rotated blocks (signs, banners, skulls)
// to store facing in 4 bits. Pure int/float arithmetic.
//
// Every value/formula is verbatim from RotationSegment.java:
//   private static final SegmentedAnglePrecision SEGMENTED_ANGLE16 =
//       new SegmentedAnglePrecision(4);
//   private static final int MAX_SEGMENT_INDEX = SEGMENTED_ANGLE16.getMask();  // 15
//   private static final int NORTH_0   = 0;
//   private static final int EAST_90   = 4;
//   private static final int SOUTH_180 = 8;
//   private static final int WEST_270  = 12;
//
// The underlying SegmentedAnglePrecision is the repo's already-certified port
// (seg_angle_parity, bit-for-bit vs the real net.minecraft class) — this header
// REUSES it and adds nothing to its arithmetic.
//
// convertToDirection returns an Optional<Direction>. To stay self-contained (avoid
// pulling in the full Direction enum), this port returns the Direction's get3DDataValue
// (data3d) on hit, or -1 when empty. The mapping is verbatim from RotationSegment.java's
// switch and Direction.java's enum table:
//   segment 0  -> NORTH (data3d 2)
//   segment 4  -> EAST  (data3d 5)
//   segment 8  -> SOUTH (data3d 3)
//   segment 12 -> WEST  (data3d 4)
//   default    -> empty (-1)

#ifndef MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_ROTATION_SEGMENT_H
#define MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_ROTATION_SEGMENT_H

#include "util/SegmentedAnglePrecision.h"

namespace mc::world::level::block::state::properties {

// RotationSegment is a class with only static members in Java; mirror that here.
class RotationSegment {
public:
    // RotationSegment.java:8 — new SegmentedAnglePrecision(4).
    static const mc::util::SegmentedAnglePrecision& segmentedAngle16() {
        static const mc::util::SegmentedAnglePrecision INSTANCE(4);
        return INSTANCE;
    }

    // RotationSegment.java:10-13 — facing-segment constants.
    static constexpr int NORTH_0 = 0;
    static constexpr int EAST_90 = 4;
    static constexpr int SOUTH_180 = 8;
    static constexpr int WEST_270 = 12;

    // RotationSegment.java:15-17 — MAX_SEGMENT_INDEX = SEGMENTED_ANGLE16.getMask().
    static int getMaxSegmentIndex() {
        return segmentedAngle16().getMask();
    }

    // RotationSegment.java:19-21 — convertToSegment(Direction).
    // Direction is supplied as its get2DDataValue() + axis.isVertical() (exactly what
    // the real SegmentedAnglePrecision.fromDirection reads off the enum).
    static int convertToSegment(int direction2DDataValue, bool axisIsVertical) {
        return segmentedAngle16().fromDirection(direction2DDataValue, axisIsVertical);
    }

    // RotationSegment.java:23-25 — convertToSegment(float rotDegrees).
    static int convertToSegment(float rotDegrees) {
        return segmentedAngle16().fromDegrees(rotDegrees);
    }

    // RotationSegment.java:27-36 — convertToDirection(int segment).
    // Returns the Direction's data3d on hit, or -1 (empty Optional) otherwise.
    static int convertToDirection(int segment) {
        switch (segment) {
            case 0:  return 2;   // NORTH
            case 4:  return 5;   // EAST
            case 8:  return 3;   // SOUTH
            case 12: return 4;   // WEST
            default: return -1;  // Optional.empty
        }
    }

    // RotationSegment.java:38-40 — convertToDegrees(int segment).
    static float convertToDegrees(int segment) {
        return segmentedAngle16().toDegrees(segment);
    }
};

}  // namespace mc::world::level::block::state::properties

#endif  // MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_ROTATION_SEGMENT_H
