// 1:1 port of net.minecraft.util.SegmentedAnglePrecision (26.1.2) — quantizes an
// angle (degrees) into a fixed-point "binary angle" of `bitPrecision` bits, where a
// full turn (360°) maps to 1<<bitPrecision. Used by RotationSegment / rotated blocks
// (signs, banners, skulls) to store facing in N bits. Pure int/float arithmetic.
//
// Every value/formula is verbatim from SegmentedAnglePrecision.java. Certified
// bit-for-bit by seg_angle_parity against the real net.minecraft class.
//
// NOTE on fromDirection: the Java method reads Direction.get2DDataValue() and
// Direction.getAxis().isVertical(). To avoid pulling in the full Direction enum,
// this port takes the 2D data value and the vertical flag as explicit inputs — the
// caller supplies exactly what Direction would have returned (NORTH=2, SOUTH=0,
// WEST=1, EAST=3; UP/DOWN are vertical with data2d=-1). The arithmetic below is
// identical to the Java body.

#ifndef MCPP_UTIL_SEGMENTED_ANGLE_PRECISION_H
#define MCPP_UTIL_SEGMENTED_ANGLE_PRECISION_H

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace mc::util {

// java.lang.Math.round(float) — the modern (JDK-8010430) impl that avoids the
// +0.5f double-rounding path. Used by fromDegreesWithTurns. Bit-identical to the JDK.
inline int javaRoundF(float a) {
    int32_t intBits = std::bit_cast<int32_t>(a);
    int biasedExp = (intBits & 0x7F800000) >> 23;
    int shift = (24 - 2 + 127) - biasedExp;            // 149 - biasedExp
    if ((shift & -32) == 0) {
        int r = (intBits & 0x007FFFFF) | 0x00800000;
        if (intBits < 0) r = -r;
        return ((r >> shift) + 1) >> 1;
    }
    // Java's else-branch is `return (int) a;`, a float->int narrowing cast that
    // SATURATES per JLS 5.1.3 (NaN->0, >=2^31 -> INT_MAX, <=-2^31 -> INT_MIN).
    // A bare static_cast<int> on an out-of-range float is UB in C++, so saturate
    // explicitly to stay bit-identical to java.lang.Math.round(float).
    if (std::isnan(a)) return 0;
    if (a >= 2147483647.0F) return 2147483647;
    if (a <= -2147483648.0F) return -2147483648;
    return static_cast<int>(a);
}

class SegmentedAnglePrecision {
public:
    // SegmentedAnglePrecision.java:11-25
    explicit SegmentedAnglePrecision(int bitPrecision) {
        if (bitPrecision < 2) {
            throw std::invalid_argument("Precision cannot be less than 2 bits");
        }
        if (bitPrecision > 30) {
            throw std::invalid_argument("Precision cannot be greater than 30 bits");
        }
        int twoPi = 1 << bitPrecision;
        mask_ = twoPi - 1;
        precision_ = bitPrecision;
        degreeToAngle_ = twoPi / 360.0F;
        angleToDegree_ = 360.0F / twoPi;
    }

    // SegmentedAnglePrecision.java:27-30
    bool isSameAxis(int binaryAngleA, int binaryAngleB) const {
        int semicircleMask = getMask() >> 1;
        return (binaryAngleA & semicircleMask) == (binaryAngleB & semicircleMask);
    }

    // SegmentedAnglePrecision.java:32-39 — split into the two real branches.
    // direction.getAxis().isVertical() -> return 0.
    // else: direction.get2DDataValue() << (precision - 2).
    int fromDirection(int direction2DDataValue, bool axisIsVertical) const {
        if (axisIsVertical) {
            return 0;
        }
        return direction2DDataValue << (precision_ - 2);
    }

    // SegmentedAnglePrecision.java:41-43
    int fromDegreesWithTurns(float degrees) const {
        return javaRoundF(degrees * degreeToAngle_);
    }

    // SegmentedAnglePrecision.java:45-47
    int fromDegrees(float degrees) const {
        return normalize(fromDegreesWithTurns(degrees));
    }

    // SegmentedAnglePrecision.java:49-51
    float toDegreesWithTurns(int binaryAngle) const {
        return binaryAngle * angleToDegree_;
    }

    // SegmentedAnglePrecision.java:53-56
    float toDegrees(int binaryAngle) const {
        float degrees = toDegreesWithTurns(normalize(binaryAngle));
        return degrees >= 180.0F ? degrees - 360.0F : degrees;
    }

    // SegmentedAnglePrecision.java:58-60
    int normalize(int binaryAngle) const {
        return binaryAngle & mask_;
    }

    // SegmentedAnglePrecision.java:62-64
    int getMask() const { return mask_; }

    // exposed for completeness / tests
    int getPrecision() const { return precision_; }
    float getDegreeToAngle() const { return degreeToAngle_; }
    float getAngleToDegree() const { return angleToDegree_; }

private:
    int mask_;
    int precision_;
    float degreeToAngle_;
    float angleToDegree_;
};

}  // namespace mc::util

#endif  // MCPP_UTIL_SEGMENTED_ANGLE_PRECISION_H
