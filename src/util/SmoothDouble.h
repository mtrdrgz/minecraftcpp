#pragma once

// 1:1 port of net.minecraft.util.SmoothDouble (Minecraft 26.1.2). STATEFUL camera
// smoothing used by Mouse/turn handling. Fields and method bodies are VERBATIM from
// 26.1.2/src/net/minecraft/util/SmoothDouble.java:
//
//   private double targetValue;
//   private double remainingValue;
//   private double lastAmount;
//
//   public double getNewDeltaValue(final double targetDelta, final double time) {
//      this.targetValue += targetDelta;
//      double delta = this.targetValue - this.remainingValue;
//      double newLastAmount = Mth.lerp(0.5, this.lastAmount, delta);
//      double deltaSign = Math.signum(delta);
//      if (deltaSign * delta > deltaSign * this.lastAmount) {
//         delta = newLastAmount;
//      }
//      this.lastAmount = newLastAmount;
//      this.remainingValue += delta * time;
//      return delta * time;
//   }
//
//   public void reset() { targetValue = remainingValue = lastAmount = 0.0; }
//
// Dependencies: Mth.lerp(double,double,double) == start + delta*(end-start), which is
// plain double arithmetic (mc::levelgen::mth::lerp, already certified bit-exact), and
// java.lang.Math.signum(double). signum is NOT libm — the JDK impl is the exact
// branch below (preserves NaN and signed zero, copySign otherwise); it reproduces
// bit-for-bit with std::copysign. So this whole class is deterministic / bit-exact.

#include <cmath>

#include "../world/level/levelgen/Mth.h"

namespace mc::util {

// java.lang.Math.signum(double d):
//   return (d == 0.0 || Double.isNaN(d)) ? d : Math.copySign(1.0, d);
// (Math.signum delegates to Math.copySign(1.0, d) for normal/inf via the same
// expression; the d==0||NaN guard returns d unchanged, preserving -0.0/+0.0/NaN.)
inline double mathSignum(double d) {
    return (d == 0.0 || std::isnan(d)) ? d : std::copysign(1.0, d);
}

class SmoothDouble {
public:
    // public double getNewDeltaValue(final double targetDelta, final double time)
    double getNewDeltaValue(double targetDelta, double time) {
        targetValue_ += targetDelta;
        double delta = targetValue_ - remainingValue_;
        double newLastAmount = mc::levelgen::mth::lerp(0.5, lastAmount_, delta);
        double deltaSign = mathSignum(delta);
        if (deltaSign * delta > deltaSign * lastAmount_) {
            delta = newLastAmount;
        }

        lastAmount_ = newLastAmount;
        remainingValue_ += delta * time;
        return delta * time;
    }

    // public void reset()
    void reset() {
        targetValue_ = 0.0;
        remainingValue_ = 0.0;
        lastAmount_ = 0.0;
    }

    // State accessors so the parity test can compare the full internal state after
    // each call (the Java GT dumps these private fields via reflection).
    double targetValue() const { return targetValue_; }
    double remainingValue() const { return remainingValue_; }
    double lastAmount() const { return lastAmount_; }

private:
    double targetValue_ = 0.0;
    double remainingValue_ = 0.0;
    double lastAmount_ = 0.0;
};

}  // namespace mc::util
