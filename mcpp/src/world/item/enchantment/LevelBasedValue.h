// 1:1 C++ port of net.minecraft.world.item.enchantment.LevelBasedValue (26.1.2).
//
// Java is a sealed-ish interface with a `float calculate(int level)` and six
// record implementations:
//   Constant(value)                       -> value
//   Linear(base, perLevelAboveFirst)      -> base + perLevelAboveFirst*(level-1)
//   LevelsSquared(added)                  -> Mth.square(level) + added   (INT square!)
//   Clamped(value, min, max)              -> Mth.clamp(value.calc, min, max)
//   Fraction(numerator, denominator)      -> den==0 ? 0 : num.calc / den.calc
//   Exponent(base, power)                 -> (float)Math.pow(base.calc, power.calc)
//   Lookup(values, fallback)              -> level<=size ? values[level-1] : fallback.calc
//
// Only `calculate` is ported here; the (codec / registry) machinery is datapack
// loading and is intentionally NOT ported (it is registry-coupled). The traps
// that matter for byte-exactness:
//   * LevelsSquared uses Mth.square(int) = int*int (wraps; UB-in-C++ if signed)
//     then PROMOTES the int result to float — NOT a float square. We compute the
//     product in uint32_t to get Java's two's-complement wrap, then widen.
//   * Linear's (level - 1) is INT subtraction (wraps), then multiplied as float.
//   * Exponent is (float)Math.pow(double,double): the pow is done in double and
//     narrowed once to float at the end.
//   * Fraction guards ONLY denominator == 0.0F exactly (so -0.0F denom -> 0,
//     because -0.0F == 0.0F in IEEE compare); division is float/float.
//   * Mth.clamp(float) is `value < min ? min : Math.min(value, max)` — note the
//     std::fmin-equivalent semantics for the upper bound only on the < path.
//
// Modeled as a tagged node tree (shared_ptr children) so nested values
// (Clamped/Fraction/Exponent wrapping other LevelBasedValues) compose exactly
// like the Java records.
#ifndef MCPP_WORLD_ITEM_ENCHANTMENT_LEVELBASEDVALUE_H
#define MCPP_WORLD_ITEM_ENCHANTMENT_LEVELBASEDVALUE_H

#include <cstdint>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace mc::world::item::enchantment {

// --- Mth helpers, ported 1:1 from net.minecraft.util.Mth (26.1.2) -----------

// Mth.clamp(float value, float min, float max):
//   return value < min ? min : Math.min(value, max);
// Java's Math.min(float,float) returns NaN if either arg is NaN and treats
// -0.0F as less than +0.0F. std::fmin does NOT propagate NaN the same way, so we
// reproduce Math.min explicitly.
inline float javaMathMin(float a, float b) {
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<float>::quiet_NaN();
    if (a == 0.0f && b == 0.0f) {
        // Both zero: Math.min returns the negative zero if either is -0.0.
        std::uint32_t ba, bb;
        std::memcpy(&ba, &a, 4);
        std::memcpy(&bb, &b, 4);
        // sign bit set => negative zero; -0.0 is "smaller".
        return (ba & 0x80000000u) ? a : b;
    }
    return a <= b ? a : b;
}

inline float mthClamp(float value, float min, float max) {
    return value < min ? min : javaMathMin(value, max);
}

// Mth.square(int x) = x * x  (int multiply, two's-complement wrap on overflow).
inline std::int32_t mthSquareInt(std::int32_t x) {
    std::uint32_t u = static_cast<std::uint32_t>(x);
    return static_cast<std::int32_t>(u * u);
}

// --- LevelBasedValue node tree ----------------------------------------------

struct LevelBasedValue;
using LBVPtr = std::shared_ptr<const LevelBasedValue>;

struct LevelBasedValue {
    enum class Kind {
        Constant,
        Linear,
        LevelsSquared,
        Clamped,
        Fraction,
        Exponent,
        Lookup,
    };

    Kind kind;

    // Constant.value / LevelsSquared.added
    float scalar = 0.0f;
    // Linear.base, Clamped.min  (reuse scalar for the single-scalar shapes)
    float base = 0.0f;
    float perLevelAboveFirst = 0.0f;
    float min = 0.0f;
    float max = 0.0f;

    // Children for the composite shapes.
    LBVPtr a;  // Clamped.value / Fraction.numerator / Exponent.base / Lookup.fallback
    LBVPtr b;  // Fraction.denominator / Exponent.power

    // Lookup.values
    std::vector<float> values;

    // calculate(int level), dispatching on kind — a 1:1 translation of each
    // record's calculate().
    float calculate(std::int32_t level) const {
        switch (kind) {
            case Kind::Constant:
                return scalar;
            case Kind::Linear:
                // base + perLevelAboveFirst * (level - 1)
                // (level - 1) is int subtraction; promoted to float for the mul.
                return base + perLevelAboveFirst *
                                  static_cast<float>(level - 1);
            case Kind::LevelsSquared:
                // Mth.square(level) [int*int] + added [float]
                return static_cast<float>(mthSquareInt(level)) + scalar;
            case Kind::Clamped:
                return mthClamp(a->calculate(level), min, max);
            case Kind::Fraction: {
                float denominator = b->calculate(level);
                return denominator == 0.0f ? 0.0f
                                           : a->calculate(level) / denominator;
            }
            case Kind::Exponent:
                // (float)Math.pow(base.calc, power.calc): pow in double, narrow once.
                return static_cast<float>(std::pow(
                    static_cast<double>(a->calculate(level)),
                    static_cast<double>(b->calculate(level))));
            case Kind::Lookup:
                // level <= values.size() ? values.get(level - 1) : fallback.calc
                // CONTRACT: enchant levels are >= 1. For level <= 0 (or any level
                // whose level-1 is out of [0,size)) the REAL Java throws
                // ArrayIndexOutOfBoundsException; here it is out-of-bounds UB. The
                // gate only exercises level >= 1 (see LevelBasedValueParity), so
                // this branch is never hit out of contract in the parity sweep.
                return (static_cast<std::size_t>(level) <= values.size())
                           ? values[static_cast<std::size_t>(level) - 1]
                           : a->calculate(level);
        }
        return 0.0f;  // unreachable
    }

    // --- factory helpers mirroring the Java constructors --------------------
    static LBVPtr constant(float value) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::Constant;
        n->scalar = value;
        return n;
    }
    static LBVPtr linear(float base, float perLevelAboveFirst) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::Linear;
        n->base = base;
        n->perLevelAboveFirst = perLevelAboveFirst;
        return n;
    }
    static LBVPtr levelsSquared(float added) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::LevelsSquared;
        n->scalar = added;
        return n;
    }
    static LBVPtr clamped(LBVPtr value, float min, float max) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::Clamped;
        n->a = std::move(value);
        n->min = min;
        n->max = max;
        return n;
    }
    static LBVPtr fraction(LBVPtr numerator, LBVPtr denominator) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::Fraction;
        n->a = std::move(numerator);
        n->b = std::move(denominator);
        return n;
    }
    static LBVPtr exponent(LBVPtr base, LBVPtr power) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::Exponent;
        n->a = std::move(base);
        n->b = std::move(power);
        return n;
    }
    static LBVPtr lookup(std::vector<float> values, LBVPtr fallback) {
        auto n = std::make_shared<LevelBasedValue>();
        n->kind = Kind::Lookup;
        n->values = std::move(values);
        n->a = std::move(fallback);
        return n;
    }
};

}  // namespace mc::world::item::enchantment

#endif  // MCPP_WORLD_ITEM_ENCHANTMENT_LEVELBASEDVALUE_H
