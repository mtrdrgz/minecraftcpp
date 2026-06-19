#pragma once
#include <cstdint>

#include "core/Vec3i.h"   // mc::Vec3i (the certified port)

// ---------------------------------------------------------------------------
// Test-local helper: net.minecraft.core.Vec3i.compareTo(Vec3i) (Vec3i.java:57-63).
//
// The certified engine header core/Vec3i.h deliberately does NOT expose
// compareTo (it is the Comparable surface, not needed by worldgen). This tiny
// header — owned by the Vec3i verify gate ONLY — ports it 1:1 so the gate can
// certify it WITHOUT editing the shared header. It lives next to the test and is
// included only by Vec3iVerifyParityTest.cpp.
//
// Java body (Vec3i.java:57-63):
//   public int compareTo(Vec3i pos) {
//      if (this.getY() == pos.getY()) {
//         return this.getZ() == pos.getZ() ? this.getX() - pos.getX()
//                                          : this.getZ() - pos.getZ();
//      } else {
//         return this.getY() - pos.getY();
//      }
//   }
//
// Every `a - b` is Java `int - int`: 32-bit two's-complement wraparound. We route
// each subtraction through uint32_t so INT_MIN/INT_MAX deltas wrap exactly like
// the JVM (e.g. 2147483647 - (-1) == INT_MIN), matching the raw int compareTo
// returns (the values are NOT clamped to {-1,0,1}).
// ---------------------------------------------------------------------------

namespace mc::vec3i_verify {

// Java `int - int` (defined two's-complement wraparound).
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

// Vec3i.compareTo(Vec3i) — Vec3i.java:57-63.
constexpr int32_t compareTo(const Vec3i& self, const Vec3i& pos) noexcept {
    if (self.getY() == pos.getY()) {
        return self.getZ() == pos.getZ()
                   ? isub(self.getX(), pos.getX())
                   : isub(self.getZ(), pos.getZ());
    }
    return isub(self.getY(), pos.getY());
}

} // namespace mc::vec3i_verify
