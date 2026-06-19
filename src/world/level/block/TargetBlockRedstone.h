#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// 1:1 C++ port of the pure redstone-strength helper of
//   net.minecraft.world.level.block.TargetBlock (Minecraft Java Edition 26.1.2)
//
//   TargetBlock.getRedstoneStrength(BlockHitResult, Vec3)   TargetBlock.java:61-77
//
// This is the math that decides how much redstone a target block emits given
// WHERE a projectile struck it: distance of the hit point from the centre of
// the struck face, mapped to [1..15]. It is a pure static helper — it reads
// ONLY the hit Direction's axis and three doubles (the hit location); it has
// NO world/BlockGetter/registry/GL/entity-state coupling, so it ports
// standalone and is exercised against the REAL class via reflection.
//
// Java source (TargetBlock.java:61-77):
//   private static int getRedstoneStrength(BlockHitResult hitResult, Vec3 hitLocation) {
//      Direction hitDirection = hitResult.getDirection();
//      double distX = Math.abs(Mth.frac(hitLocation.x) - 0.5);
//      double distY = Math.abs(Mth.frac(hitLocation.y) - 0.5);
//      double distZ = Math.abs(Mth.frac(hitLocation.z) - 0.5);
//      Direction.Axis axis = hitDirection.getAxis();
//      double distance;
//      if (axis == Direction.Axis.Y)      distance = Math.max(distX, distZ);
//      else if (axis == Direction.Axis.Z) distance = Math.max(distX, distY);
//      else                               distance = Math.max(distY, distZ);
//      return Math.max(1, Mth.ceil(15.0 * Mth.clamp((0.5 - distance) / 0.5, 0.0, 1.0)));
//   }
//
// 1:1 TRAPS reproduced exactly:
//   * Mth.frac(double) = num - (long)Math.floor(num)  — truncation through long,
//     NOT std::fmod; differs for negatives and at integer boundaries.
//   * Math.abs(double): IEEE-754 magnitude (handles -0.0 -> +0.0).
//   * Math.max(double): NOT std::max — Java semantics propagate NaN and treat
//     +0.0 > -0.0; reproduced via javaMaxD.
//   * Mth.clamp(double v,min,max) = v<min ? min : Math.min(v,max)  — order of the
//     comparisons matters for NaN; reproduced via mthClampD.
//   * Mth.ceil(double) = (int)Math.ceil(v)  — ceil then narrow-to-int (truncation
//     toward zero on the already-integral ceil result).
//   * Math.max(1, ...): integer max.
//
// Axis ordinals match Java (Direction.Axis: X=0, Y=1, Z=2 — Direction.java:402-480).
// ---------------------------------------------------------------------------

namespace mc::block_target {

// Java: net.minecraft.core.Direction.Axis ordinals (X=0, Y=1, Z=2).
enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

// Java: net.minecraft.core.Direction ordinals (Direction.java:33-38):
//   DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// Java: Direction.getAxis() — DOWN/UP -> Y, NORTH/SOUTH -> Z, WEST/EAST -> X.
constexpr Axis directionAxis(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN:
        case Direction::UP:    return Axis::Y;
        case Direction::NORTH:
        case Direction::SOUTH: return Axis::Z;
        default:               return Axis::X; // WEST, EAST
    }
}

// Java: net.minecraft.util.Mth.lfloor(double) = (long)Math.floor(v)  (Mth.java:69-71).
inline int64_t mthLfloor(double v) noexcept {
    return static_cast<int64_t>(std::floor(v));
}

// Java: Mth.frac(double num) = num - lfloor(num)  (Mth.java:304-306).
inline double mthFrac(double num) noexcept {
    return num - static_cast<double>(mthLfloor(num));
}

// Java: java.lang.Math.max(double a, double b). Distinct from std::max for NaN
// (propagates) and signed zero (+0.0 considered greater than -0.0).
inline double javaMaxD(double a, double b) noexcept {
    if (a != a) return a;          // a is NaN
    if (b != b) return b;          // b is NaN
    if (a == 0.0 && b == 0.0) {    // both zero: pick +0.0 over -0.0
        // std::signbit -> true for negative zero. Math.max(+0,-0) == +0.
        return std::signbit(a) ? b : a;
    }
    return a > b ? a : b;
}

// Java: net.minecraft.util.Mth.clamp(double value, double min, double max)
//   = value < min ? min : Math.min(value, max)  (Mth.java:105-107).
inline double mthClampD(double value, double mn, double mx) noexcept {
    if (value < mn) return mn;
    // Math.min(value, max): NaN-propagating, signed-zero-aware. For this helper's
    // finite domain it reduces to the ordinary minimum, but mirror Java precisely.
    if (value != value || mx != mx) return value != value ? value : mx; // NaN handling
    if (value == 0.0 && mx == 0.0) return std::signbit(value) ? value : mx; // min -> -0.0
    return value < mx ? value : mx;
}

// Java: Mth.ceil(double v) = (int)Math.ceil(v)  (Mth.java:85-87).
inline int32_t mthCeil(double v) noexcept {
    return static_cast<int32_t>(std::ceil(v));
}

// 1:1 port of TargetBlock.getRedstoneStrength(BlockHitResult, Vec3).
// `hitDir` supplies only its axis (Java reads hitResult.getDirection().getAxis()),
// and (x,y,z) is the hit location (Java's Vec3 hitLocation).
inline int32_t getRedstoneStrength(Direction hitDir, double x, double y, double z) noexcept {
    const double distX = std::abs(mthFrac(x) - 0.5);
    const double distY = std::abs(mthFrac(y) - 0.5);
    const double distZ = std::abs(mthFrac(z) - 0.5);
    const Axis axis = directionAxis(hitDir);
    double distance;
    if (axis == Axis::Y) {
        distance = javaMaxD(distX, distZ);
    } else if (axis == Axis::Z) {
        distance = javaMaxD(distX, distY);
    } else { // X
        distance = javaMaxD(distY, distZ);
    }
    const int32_t scaled = mthCeil(15.0 * mthClampD((0.5 - distance) / 0.5, 0.0, 1.0));
    return std::max(1, scaled);
}

} // namespace mc::block_target
