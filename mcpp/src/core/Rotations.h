#pragma once

// 1:1 port of net.minecraft.core.Rotations (26.1.2).
//
// Source (26.1.2/src/net/minecraft/core/Rotations.java):
//   public record Rotations(float x, float y, float z) {
//      public Rotations {
//         x = !Float.isInfinite(x) && !Float.isNaN(x) ? x % 360.0F : 0.0F;
//         y = !Float.isInfinite(y) && !Float.isNaN(y) ? y % 360.0F : 0.0F;
//         z = !Float.isInfinite(z) && !Float.isNaN(z) ? z % 360.0F : 0.0F;
//      }
//   }
//
// The canonical constructor normalizes each component into (-360,360) by the Java
// float remainder operator `%` (truncated-division remainder == std::fmod for
// floats; both follow IEEE-754). Non-finite inputs (Inf/NaN) collapse to 0.0F.
// The record accessors x()/y()/z() simply return the already-normalized fields.
//
// NOTE: 26.1.2 has NO getWrappedX/Y/Z methods (those were an older API). The wrap
// helper for that older surface was Mth.wrapDegrees(float), which is already
// certified bit-exact at mc::levelgen::mth::wrapDegrees and is reused (not
// duplicated) by the parity gate.

#include <cmath>

namespace mc::core {

class Rotations {
public:
    // Canonical constructor: replicates the Java record's normalization verbatim.
    Rotations(float x, float y, float z)
        : x_(normalize(x)), y_(normalize(y)), z_(normalize(z)) {}

    // Record accessors (Java x()/y()/z()).
    float x() const { return x_; }
    float y() const { return y_; }
    float z() const { return z_; }

    // The per-component normalization used by the canonical constructor:
    //   !isInfinite(v) && !isNaN(v) ? v % 360.0F : 0.0F
    static float normalize(float v) {
        return (!std::isinf(v) && !std::isnan(v)) ? std::fmod(v, 360.0F) : 0.0F;
    }

private:
    float x_;
    float y_;
    float z_;
};

} // namespace mc::core
