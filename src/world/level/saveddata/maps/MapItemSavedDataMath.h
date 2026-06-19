// 1:1 C++ port of the PURE STATIC coordinate / decoration math of
// net.minecraft.world.level.saveddata.maps.MapItemSavedData (26.1.2).
//
// Java source: 26.1.2/src/net/minecraft/world/level/saveddata/maps/MapItemSavedData.java
//
// This header ports ONLY the self-contained, world-free integer/float math that
// answers "given a world (x,z), a map scale and a map center, where on the map
// (and how) does a point land". None of these read any Level / registry / entity
// tick state / GL — they are deterministic functions of their scalar arguments.
// Certified bit-exact against the REAL net.minecraft.world.level.saveddata.maps
// .MapItemSavedData via tools/MapItemSavedDataMathParity.java +
// MapItemSavedDataMathParityTest.cpp.
//
// PORTED (each maps 1:1 onto the Java body cited):
//
//   * calculateMapCenter(originX, originY, scale) -> {centerX, centerZ}
//       The integer center math inside MapItemSavedData.createFresh (lines 131-145):
//           int size  = 128 * (1 << scale);
//           int areaX = Mth.floor((originX + 64.0) / size);
//           int areaZ = Mth.floor((originY + 64.0) / size);
//           int x = areaX * size + size / 2 - 64;
//           int z = areaZ * size + size / 2 - 64;
//       createFresh wraps those five lines in a `new MapItemSavedData(x, z, ...)`;
//       the world-coupled object is irrelevant to the math, so only the (x,z)
//       computation is ported. (GT drives the REAL createFresh and reads its public
//       centerX/centerZ, so the wrapper is still exercised end-to-end.)
//
//   * isInsideMap(xd, yd)         -- MapItemSavedData.isInsideMap (lines 340-343):
//       float halfSize 63; xd>=-63 && yd>=-63 && xd<=63 && yd<=63.
//
//   * clampMapCoordinate(delta)   -- MapItemSavedData.clampMapCoordinate (355-362):
//       delta<=-63 -> -128; delta>=63 -> 127; else (byte)(delta*2.0F + 0.5).
//       The classic float->byte truncation-toward-zero + 0.5 bias trap.
//
//   * calculateRotationOverworld(yRot) -- the NON-Nether branch of
//       MapItemSavedData.calculateRotation (lines 330-338):
//           double adjustedYRot = yRot < 0.0 ? yRot - 8.0 : yRot + 8.0;
//           return (byte)(adjustedYRot * 16.0 / 360.0);
//       (double->byte truncation toward zero.) The Nether branch is time/level
//       coupled (level.getGameTime()) so it is intentionally NOT ported here.
//
//   * decorationDeltaFromCenter(worldCoord, center, scale) -- the per-axis delta
//       computed at the top of MapItemSavedData.addDecoration (lines 275-277):
//           int scaling = 1 << this.scale;
//           float xDeltaFromCenter = (float)(xPos - this.centerX) / scaling;
//       i.e. ((float)(worldCoord - center)) / (1 << scale).
//
// NOT PORTED (out of scope for a pure-math gate; world/registry/entity coupled):
//   * calculateRotation Nether branch  -- needs LevelAccessor.getGameTime().
//   * addDecoration / calculateDecorationLocationAndType / player-off-map logic --
//     touch MapDecorationType holders, the live decorations map, trackingPosition,
//     unlimitedTracking instance state, and Level dimension checks.
//
// 1:1 TRAPS captured here:
//   - `1 << scale` widening: scale is small (0..4 in vanilla) so it fits an int;
//     done in 32-bit signed to match Java's int shift.
//   - integer `size / 2` floor-division and `areaX * size` happen in 32-bit signed.
//   - Mth.floor = (int)Math.floor(double): floor toward -inf THEN narrow to int.
//   - float division `(float)(world - center) / scaling`: the subtraction is done
//     as a Java int, widened to float, then divided by an int promoted to float.
//   - clampMapCoordinate / calculateRotationOverworld: Java narrowing casts to byte
//     truncate toward zero and then keep only the low 8 bits (two's complement).
#ifndef MCPP_WORLD_LEVEL_SAVEDDATA_MAPS_MAPITEMSAVEDDATAMATH_H
#define MCPP_WORLD_LEVEL_SAVEDDATA_MAPS_MAPITEMSAVEDDATAMATH_H

#include <cmath>
#include <cstdint>

namespace mc::world::level::saveddata::maps {

// --- Mth helpers, replicated verbatim from net.minecraft.util.Mth (26.1.2) so
// --- this gate is dependency-free. ---------------------------------------------

// Mth.floor(double): (int)Math.floor(v). Math.floor returns the largest double
// <= v that is an integer; the narrowing cast to int then truncates toward zero,
// but since the value is already integral the result is floor toward -inf,
// modulo Java's (int) overflow behaviour for out-of-range doubles.
inline std::int32_t mth_floor(double v) {
    return static_cast<std::int32_t>(std::floor(v));
}

// Mth.clamp(int, int, int): Math.min(Math.max(value, min), max).
inline std::int32_t mth_clamp(std::int32_t value, std::int32_t lo, std::int32_t hi) {
    std::int32_t r = value < lo ? lo : value;
    return r > hi ? hi : r;
}

// Result of the createFresh center computation.
struct MapCenter {
    std::int32_t centerX;
    std::int32_t centerZ;
};

// MapItemSavedData.createFresh(originX, originY, scale) center math (lines 139-143).
// `scale` is a Java byte; pass the already sign-extended value.
inline MapCenter calculateMapCenter(double originX, double originY, std::int32_t scale) {
    // int size = 128 * (1 << scale);
    // 1 << scale is a 32-bit signed shift in Java; do the same here.
    std::int32_t size = 128 * (1 << scale);
    // int areaX = Mth.floor((originX + 64.0) / size);
    std::int32_t areaX = mth_floor((originX + 64.0) / static_cast<double>(size));
    std::int32_t areaZ = mth_floor((originY + 64.0) / static_cast<double>(size));
    // int x = areaX * size + size / 2 - 64;   (all 32-bit signed arithmetic)
    // Use unsigned intermediates for the multiply/add so a Java int overflow wraps
    // (two's complement) instead of being C++ signed-overflow UB at -O2.
    std::uint32_t ux =
        static_cast<std::uint32_t>(areaX) * static_cast<std::uint32_t>(size)
        + static_cast<std::uint32_t>(size / 2) - 64u;
    std::uint32_t uz =
        static_cast<std::uint32_t>(areaZ) * static_cast<std::uint32_t>(size)
        + static_cast<std::uint32_t>(size / 2) - 64u;
    return MapCenter{static_cast<std::int32_t>(ux), static_cast<std::int32_t>(uz)};
}

// MapItemSavedData.isInsideMap(float xd, float yd) (lines 340-343).
inline bool isInsideMap(float xd, float yd) {
    return xd >= -63.0f && yd >= -63.0f && xd <= 63.0f && yd <= 63.0f;
}

// MapItemSavedData.clampMapCoordinate(float deltaFromCenter) (lines 355-362).
// Returns a Java byte (signed 8-bit).
inline std::int8_t clampMapCoordinate(float deltaFromCenter) {
    if (deltaFromCenter <= -63.0f) {
        return static_cast<std::int8_t>(-128);
    }
    if (deltaFromCenter >= 63.0f) {
        return static_cast<std::int8_t>(127);
    }
    // (byte)(deltaFromCenter * 2.0F + 0.5): the float*2+0.5 happens in float, then
    // the result is narrowed to byte. Java (byte) on a float = (byte)(int)float,
    // i.e. truncate toward zero to int, keep low 8 bits.
    float f = deltaFromCenter * 2.0f + 0.5f;
    std::int32_t asInt = static_cast<std::int32_t>(f);  // truncate toward zero
    return static_cast<std::int8_t>(asInt);             // keep low 8 bits
}

// MapItemSavedData.calculateRotation NON-Nether branch (lines 335-336).
// Returns a Java byte.
inline std::int8_t calculateRotationOverworld(double yRot) {
    double adjustedYRot = yRot < 0.0 ? yRot - 8.0 : yRot + 8.0;
    // (byte)(adjustedYRot * 16.0 / 360.0): double*const, narrow to byte =
    // (byte)(int)double : truncate toward zero to int, keep low 8 bits.
    double d = adjustedYRot * 16.0 / 360.0;
    std::int32_t asInt = static_cast<std::int32_t>(d);  // truncate toward zero
    return static_cast<std::int8_t>(asInt);
}

// Per-axis delta-from-center at the top of MapItemSavedData.addDecoration
// (lines 275-277): ((float)(worldCoord - center)) / (1 << scale).
// worldCoord/center are Java doubles in vanilla (xPos/zPos), but the center stored
// on the instance is an int; here we expose the general double form used by the
// callsite. scaling = 1 << scale is an int promoted to float for the divide.
inline float decorationDeltaFromCenter(double worldCoord, double center, std::int32_t scale) {
    std::int32_t scaling = 1 << scale;
    return static_cast<float>(worldCoord - center) / static_cast<float>(scaling);
}

}  // namespace mc::world::level::saveddata::maps

#endif  // MCPP_WORLD_LEVEL_SAVEDDATA_MAPS_MAPITEMSAVEDDATAMATH_H
