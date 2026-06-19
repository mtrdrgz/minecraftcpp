#pragma once

// Local helpers for the mth_color_verify gate. The PURPOSE of this gate is to
// VERIFY the already-certified color/hash helpers in world/level/levelgen/Mth.h
// (mc::levelgen::mth::{hsvToRgb,hsvToArgb,murmurHash3Mixer,binarySearch,
//  frac(float/double),smoothstep,smoothstepDerivative}) bit-for-bit against ground
// truth from the REAL net.minecraft.util.Mth 26.1.2 (tools/MthColorVerifyParity.java).
// We do NOT re-port those methods here — the test #includes Mth.h and calls them.
//
// The one thing this header adds is a CORRECTED reference for net.minecraft.util
// .ARGB.color(int,int,int,int), used only to demonstrate (over the FULL int range
// of alpha) the masking that the engine header's argbColor() omits. See notes:
// the engine's mth::hsvToArgb is faithful for physical alpha in [0,255] (the gate
// proves it), but ARGB.color masks every channel with & 0xFF, which the engine's
// argbColor does not. Out-of-[0,255] alpha is non-physical for a colour, so it
// never reaches hsvToArgb in practice; we record the divergence here rather than
// editing the shared header.

#include <cstdint>

namespace mc::levelgen::mth_color_verify {

// net.minecraft.util.ARGB.color(alpha,red,green,blue) — ARGB.java:68-70, VERBATIM:
//   (alpha & 0xFF) << 24 | (red & 0xFF) << 16 | (green & 0xFF) << 8 | blue & 0xFF
inline int argbColorMasked(int alpha, int red, int green, int blue) {
    return ((alpha & 0xFF) << 24) | ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
}

} // namespace mc::levelgen::mth_color_verify
