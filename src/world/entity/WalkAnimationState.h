// 1:1 C++ port of net.minecraft.world.entity.WalkAnimationState (Minecraft 26.1.2).
//
// Verbatim translation of the decompiled Java. The class is a tiny stateful float
// accumulator driving limb-swing animation:
//
//   private float speedOld;
//   private float speed;
//   private float position;
//   private float positionScale = 1.0F;
//
// All arithmetic is single-precision float, matching the Java source exactly.
// The only external dependencies are:
//   * Mth.lerp(float alpha1, float p0, float p1) = p0 + alpha1 * (p1 - p0)   (Mth.java:532-534)
//   * java.lang.Math.min(float, float)
// both of which are reproduced here so this header is self-contained.
//
// Source: 26.1.2/src/net/minecraft/world/entity/WalkAnimationState.java
//
// Parity is enforced by WalkAnimParityTest.cpp against the real Java class
// (ground truth from tools/WalkAnimParity.java), compared bit-for-bit.

#pragma once

namespace mc::world::entity {

// Mth.lerp(float, float, float) — Mth.java:532-534 — return p0 + alpha1 * (p1 - p0).
inline float mthLerpF(float alpha1, float p0, float p1) { return p0 + alpha1 * (p1 - p0); }

// java.lang.Math.min(float, float) replicated. NOTE on Java semantics:
//   - if either argument is NaN the result is NaN,
//   - -0.0F is considered strictly smaller than +0.0F.
// The parity battery uses finite, physical inputs and never -0.0F, so the plain
// "a <= b ? a : b" branch below is exact for those; the NaN/-0.0 handling is
// included only for faithfulness.
inline float javaMinF(float a, float b) {
    if (a != a) return a;  // a is NaN
    if (b != b) return b;  // b is NaN
    // -0.0 < +0.0 in Java's Math.min: distinguish via raw bit comparison.
    if (a == 0.0f && b == 0.0f) {
        // pick the one with the sign bit set (i.e. negative zero) as the minimum.
        // signbit-style check using a union-free trick: 1.0f / a is -inf for -0.0f.
        bool aNeg = (1.0f / a) < 0.0f;
        return aNeg ? a : b;
    }
    return (a <= b) ? a : b;
}

class WalkAnimationState {
public:
    // setSpeed(float speed): this.speed = speed;
    void setSpeed(float speed) { speed_ = speed; }

    // update(targetSpeed, factor, positionScale):
    //   this.speedOld = this.speed;
    //   this.speed = this.speed + (targetSpeed - this.speed) * factor;
    //   this.position = this.position + this.speed;
    //   this.positionScale = positionScale;
    void update(float targetSpeed, float factor, float positionScale) {
        speedOld_ = speed_;
        speed_ = speed_ + (targetSpeed - speed_) * factor;
        position_ = position_ + speed_;
        positionScale_ = positionScale;
    }

    // stop(): zeroes speedOld, speed, position (positionScale is NOT reset).
    void stop() {
        speedOld_ = 0.0f;
        speed_ = 0.0f;
        position_ = 0.0f;
    }

    // speed(): return this.speed;
    float speed() const { return speed_; }

    // speed(partialTicks): return Math.min(Mth.lerp(partialTicks, speedOld, speed), 1.0F);
    float speed(float partialTicks) const {
        return javaMinF(mthLerpF(partialTicks, speedOld_, speed_), 1.0f);
    }

    // position(): return this.position * this.positionScale;
    float position() const { return position_ * positionScale_; }

    // position(partialTicks):
    //   return (this.position - this.speed * (1.0F - partialTicks)) * this.positionScale;
    float position(float partialTicks) const {
        return (position_ - speed_ * (1.0f - partialTicks)) * positionScale_;
    }

    // isMoving(): return this.speed > 1.0E-5F;
    bool isMoving() const { return speed_ > 1.0e-5f; }

private:
    float speedOld_ = 0.0f;
    float speed_ = 0.0f;
    float position_ = 0.0f;
    float positionScale_ = 1.0f;
};

}  // namespace mc::world::entity
