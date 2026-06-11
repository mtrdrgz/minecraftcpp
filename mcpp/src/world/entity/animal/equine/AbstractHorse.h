#pragma once

// 1:1 port of the pure, world-free attribute-generation helpers of
// net.minecraft.world.entity.animal.equine.AbstractHorse (26.1.2).
//
// These are the static helpers that roll a horse's base MAX_HEALTH,
// MOVEMENT_SPEED and JUMP_STRENGTH from a RandomSource, plus the breeding
// helper createOffspringAttribute() that mixes two parents' attribute values
// with an RNG "baby quality" draw. Everything here is deterministic numeric
// arithmetic over a RandomSource stream — no Level, no entity, no registries —
// so it is exact ground truth for a byte-for-byte parity gate.
//
// Java source (AbstractHorse.java):
//
//   protected static float generateMaxHealth(IntUnaryOperator op) {
//      return 15.0F + op.applyAsInt(8) + op.applyAsInt(9);
//   }
//   protected static double generateJumpStrength(DoubleSupplier p) {
//      return 0.4F + p.getAsDouble()*0.2 + p.getAsDouble()*0.2 + p.getAsDouble()*0.2;
//   }
//   protected static double generateSpeed(DoubleSupplier p) {
//      return (0.45F + p.getAsDouble()*0.3 + p.getAsDouble()*0.3 + p.getAsDouble()*0.3) * 0.25;
//   }
//   static double createOffspringAttribute(double pa, double pb,
//            double rangeMin, double rangeMax, RandomSource random) { ... }
//
// In the live randomizeAttributes() the IntUnaryOperator is `random::nextInt`
// (so op(8) == random.nextInt(8)) and the DoubleSupplier is
// `random::nextDouble`. The MIN_*/MAX_* range constants are computed from these
// same helpers with op(b)->0 / op(b)->b-1 and p()->0.0 / p()->1.0.
//
// 1:1 TRAPS captured below:
//   * Float-literal widening: `0.4F` / `0.45F` are FLOAT literals that widen to
//     double in `0.4F + (double)`. (double)0.4F != 0.4 exactly, so we MUST use
//     static_cast<double>(0.4f), NOT the double literal 0.4.
//   * generateMaxHealth returns FLOAT: `15.0F + int + int` is evaluated in
//     float (int operands widen to float), so we add as float.
//   * createOffspringAttribute draws random.nextDouble() THREE times in
//     left-to-right order; C++ does not order operands within one expression, so
//     we sequence the draws into named variables.
//   * Mth.clamp(double) == value<min?min:Math.min(value,max) (NOT std::clamp's
//     ordering); we route through the certified mc::levelgen::mth::clamp.
//   * Math.abs(double) on finite inputs == std::fabs.

#include "world/level/levelgen/Mth.h"
#include "world/level/levelgen/RandomSource.h"

#include <cmath>
#include <cstdint>

namespace mc::world::entity::animal::equine {

// generateMaxHealth(IntUnaryOperator) with op == random.nextInt(bound).
// Returns FLOAT (Java return type), evaluated in float arithmetic.
inline float generateMaxHealthFromRandom(mc::levelgen::RandomSource& random) {
    int32_t a = random.nextInt(8);
    int32_t b = random.nextInt(9);
    return 15.0F + a + b;
}

// generateMaxHealth(IntUnaryOperator) for a fixed bound->value mapping (used by
// the MIN_HEALTH = op->0 and MAX_HEALTH = op->(b-1) range constants).
inline float generateMaxHealthFromOp(int32_t v8, int32_t v9) {
    return 15.0F + v8 + v9;
}

// generateJumpStrength(DoubleSupplier) with p == random.nextDouble().
inline double generateJumpStrengthFromRandom(mc::levelgen::RandomSource& random) {
    double d0 = random.nextDouble();
    double d1 = random.nextDouble();
    double d2 = random.nextDouble();
    return static_cast<double>(0.4F) + d0 * 0.2 + d1 * 0.2 + d2 * 0.2;
}

// generateJumpStrength(DoubleSupplier) for a constant supplier (range consts).
inline double generateJumpStrengthFromConst(double p) {
    return static_cast<double>(0.4F) + p * 0.2 + p * 0.2 + p * 0.2;
}

// generateSpeed(DoubleSupplier) with p == random.nextDouble().
inline double generateSpeedFromRandom(mc::levelgen::RandomSource& random) {
    double d0 = random.nextDouble();
    double d1 = random.nextDouble();
    double d2 = random.nextDouble();
    return (static_cast<double>(0.45F) + d0 * 0.3 + d1 * 0.3 + d2 * 0.3) * 0.25;
}

// generateSpeed(DoubleSupplier) for a constant supplier (range consts).
inline double generateSpeedFromConst(double p) {
    return (static_cast<double>(0.45F) + p * 0.3 + p * 0.3 + p * 0.3) * 0.25;
}

// createOffspringAttribute(parentAValue, parentBValue, attributeRangeMin,
// attributeRangeMax, random). The Java method throws
// IllegalArgumentException when attributeRangeMax <= attributeRangeMin; our
// parity battery never feeds such a case, so we only port the numeric branch.
inline double createOffspringAttribute(double parentAValue,
                                       double parentBValue,
                                       double attributeRangeMin,
                                       double attributeRangeMax,
                                       mc::levelgen::RandomSource& random) {
    parentAValue = mc::levelgen::mth::clamp(parentAValue, attributeRangeMin, attributeRangeMax);
    parentBValue = mc::levelgen::mth::clamp(parentBValue, attributeRangeMin, attributeRangeMax);
    double margin = 0.15 * (attributeRangeMax - attributeRangeMin);
    double range = std::fabs(parentAValue - parentBValue) + margin * 2.0;
    double average = (parentAValue + parentBValue) / 2.0;
    double d0 = random.nextDouble();
    double d1 = random.nextDouble();
    double d2 = random.nextDouble();
    double babyQuality = (d0 + d1 + d2) / 3.0 - 0.5;
    double newValue = average + range * babyQuality;
    if (newValue > attributeRangeMax) {
        double difference = newValue - attributeRangeMax;
        return attributeRangeMax - difference;
    } else if (newValue < attributeRangeMin) {
        double difference = attributeRangeMin - newValue;
        return attributeRangeMin + difference;
    } else {
        return newValue;
    }
}

} // namespace mc::world::entity::animal::equine
