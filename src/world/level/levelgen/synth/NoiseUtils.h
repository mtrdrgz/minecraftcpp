#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.synth.NoiseUtils (26.1.2).
//
// The real class has exactly three public static methods:
//
//   double biasTowardsExtreme(double noise, double factor)
//       return noise + Math.sin(Math.PI * noise) * factor / Math.PI;
//
//   void parityNoiseOctaveConfigString(StringBuilder, double, double, double, byte[])
//   void parityNoiseOctaveConfigString(StringBuilder, double, double, double, int[])
//       — debug String.format helpers; pure formatting, not ported (no callers in
//         worldgen math, only diagnostic strings). Listed as unported.
//
// biasTowardsExtreme is pure double arithmetic. NOTE: it uses java.lang.Math.sin
// (libm), NOT Mth.sin (the 64Ki sine TABLE). So this port MUST use std::sin, not
// mc::levelgen::mth::sin. Math.PI is the exact IEEE-754 double 3.141592653589793,
// reproduced here by the same literal. The C++ std::sin on this mingw libm equals
// Java StrictMath.sin (fdlibm) bit-for-bit, but HotSpot's java.lang.Math.sin uses a
// separate x86 INTRINSIC that differs from fdlibm by 1 ULP on a few arguments
// (verified: std::sin never equals Math.sin on those). That intrinsic is not
// reproducible by any portable libm, so the result can differ from the ground truth
// by 1 ULP (amplified to <=8 ULP only where noise and sin(...)*factor/PI nearly
// cancel to ~0). This is an unavoidable transcendental difference, NOT a port bug;
// biasTowardsExtreme has no 26.1.2 worldgen callers, so it never affects chunk
// parity. NoiseUtilsParityTest therefore accepts a documented <=8 ULP tolerance
// here (still bit-exact on the other ~99% of rows and on every non-sin operation).

#include <cmath>

namespace mc::levelgen::synth {

// Math.PI — the closest double to pi (0x400921FB54442D18). Same literal Java uses.
inline constexpr double PI = 3.141592653589793;

// NoiseUtils.biasTowardsExtreme(double noise, double factor):
//   return noise + Math.sin(Math.PI * noise) * factor / Math.PI;
// Operation order is fixed by Java precedence/associativity: (Math.sin(PI*noise))
// is multiplied by factor first, then divided by PI, then added to noise.
inline double biasTowardsExtreme(double noise, double factor) {
    return noise + std::sin(PI * noise) * factor / PI;
}

}  // namespace mc::levelgen::synth
