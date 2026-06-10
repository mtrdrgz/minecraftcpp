#pragma once

// Local helper for the mth_extra_parity gate. This header is NEW and owned by the
// gate; it does NOT modify the shared engine header Mth.h.
//
// Reason it exists: net.minecraft.util.Mth.length(float,float) (Mth.java:684-686) is
//
//     public static float length(float x, float y) {
//         return (float)Math.sqrt(lengthSquared(x, y));
//     }
//
// where lengthSquared resolves to the DOUBLE overload (Mth.java:676-678) because no
// float(float,float) lengthSquared exists — so the float args are promoted to double
// and the sum-of-squares is computed in FULL double precision before the sqrt.
//
// The shared Mth.h::length(float,float) instead computes `x * x + y * y` in FLOAT and
// only then widens to double, which loses precision and diverges from Java by 1 ULP on
// inputs like (100.0f, 9.99f): Java 0x42c8fedb vs header 0x42c8feda. This corrected
// helper reproduces the EXACT Java promotion order so the gate certifies real behavior.
// (Tracked as a header bug to fix in mc::levelgen::mth — see test notes.)

#include <cmath>

namespace mc::levelgen::mth_extra {

// Bit-exact net.minecraft.util.Mth.length(float, float): promote to double FIRST,
// compute the squared sum in double, sqrt in double, then narrow to float.
inline float lengthF(float x, float y) {
    const double dx = static_cast<double>(x);
    const double dy = static_cast<double>(y);
    return static_cast<float>(std::sqrt(dx * dx + dy * dy));
}

}  // namespace mc::levelgen::mth_extra
