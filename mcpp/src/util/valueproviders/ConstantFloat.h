#pragma once

// Faithful 1:1 port of net.minecraft.util.valueproviders.ConstantFloat
// (26.1.2/src/net/minecraft/util/valueproviders/ConstantFloat.java).
//
//   public record ConstantFloat(float value) implements FloatProvider {
//      public static final ConstantFloat ZERO = new ConstantFloat(0.0F);
//      public static ConstantFloat of(final float value) {
//         return value == 0.0F ? ZERO : new ConstantFloat(value);
//      }
//      public float sample(RandomSource random) { return this.value; }
//      public float min() { return this.value; }
//      public float max() { return this.value; }
//   }
//
// The existing engine port mc::valueproviders::ConstantFloat
// (world/level/levelgen/FloatProvider.h) only exposes sample() and stores the
// raw value, so it does NOT model min()/max() nor the of() singleton folding.
// This header adds the missing pieces faithfully so the full class can be gated.
//
// NOTE on the of() singleton: Java's `value == 0.0F` uses IEEE float equality,
// where -0.0F == 0.0F is TRUE. So of(-0.0F) returns ZERO, whose value() is
// +0.0F (raw bits 0x00000000) — NOT -0.0F (0x80000000). This is a genuine
// bit-exact subtlety the parity test must capture.

#include "../../world/level/levelgen/RandomSource.h"

namespace mc::valueproviders {

using mc::levelgen::RandomSource;

// Standalone value-semantics port of ConstantFloat. Kept independent of the
// FloatProvider virtual base in FloatProvider.h on purpose: this is the
// gate-faithful record, including of()/min()/max(); FloatProvider.h's class is
// the runtime sampler. They agree on sample() (verified in the parity test).
class ConstantFloatRecord {
public:
    // public record ConstantFloat(float value): the canonical constructor stores
    // the value verbatim (no folding — folding happens only in of()).
    explicit ConstantFloatRecord(float value) : m_value(value) {}

    // public static ConstantFloat of(final float value)
    //   return value == 0.0F ? ZERO : new ConstantFloat(value);
    // IEEE compare: -0.0F == 0.0F is true, so of(-0.0F) collapses to ZERO whose
    // value is +0.0F. NaN == 0.0F is false, so NaN passes through unchanged.
    static ConstantFloatRecord of(float value) {
        return (value == 0.0F) ? ConstantFloatRecord(0.0F)  // ZERO
                               : ConstantFloatRecord(value);
    }

    // public float value()
    float value() const { return m_value; }

    // public float sample(RandomSource random) { return this.value; }
    // (RNG is intentionally ignored — does not advance the stream.)
    float sample(RandomSource& /*random*/) const { return m_value; }
    float sample() const { return m_value; }

    // public float min() { return this.value; }
    float min() const { return m_value; }

    // public float max() { return this.value; }
    float max() const { return m_value; }

private:
    float m_value;
};

} // namespace mc::valueproviders
