// 1:1 C++ port of net.minecraft.world.entity.AnimationState (Minecraft 26.1.2).
//
// Verbatim translation of the decompiled Java. The class is a tiny stateful
// tick-based state machine driving keyframe (entity) animations:
//
//   private static final int STOPPED = Integer.MIN_VALUE;
//   private int startTick = Integer.MIN_VALUE;
//
// The whole class hangs off a single int (startTick). It is "started" iff
// startTick != Integer.MIN_VALUE. All accumulated-elapsed-time math is exact
// int/float arithmetic, matching the Java source bit-for-bit.
//
// Methods and their Java bodies (26.1.2/src/net/minecraft/world/entity/AnimationState.java):
//   start(tickCount)               -> startTick = tickCount
//   startIfStopped(tickCount)      -> if (!isStarted()) start(tickCount)
//   animateWhen(condition, tick)   -> condition ? startIfStopped(tick) : stop()
//   stop()                         -> startTick = Integer.MIN_VALUE
//   fastForward(ticks, timeScale)  -> if (isStarted()) startTick -= (int)(ticks*timeScale)
//   getTimeInMillis(ageInTicks)    -> (long)((ageInTicks - startTick) * 50.0F)
//   isStarted()                    -> startTick != Integer.MIN_VALUE
//   copyFrom(state)                -> startTick = state.startTick
//
// NOT ported (no observable output beyond the above, requires a Java
// functional Consumer<AnimationState>):
//   ifStarted(Consumer<AnimationState> timer)  -- pure callback dispatch; its
//   only effect is "call back when isStarted()", already covered by isStarted().
//
// CONVERSION NOTES (Java -> C++ bit-exactness):
//   * fastForward computes (int)(ticks * timeScale): an int*float promoted to
//     float, then a float->int narrowing cast that SATURATES per JLS 5.1.3
//     (NaN->0, >=2^31->INT_MAX, <=-2^31->INT_MIN). Reproduced via javaF2I.
//     The subtraction startTick -= ... is a Java int subtraction, i.e. modular
//     two's-complement; reproduced with int32 wraparound semantics.
//   * getTimeInMillis computes (ageInTicks - startTick) in float (startTick is
//     widened int->float, lossy for |startTick| > 2^24), * 50.0F in float, then
//     a float->long narrowing cast that SATURATES per JLS 5.1.3
//     (NaN->0, >=2^63->LONG_MAX, <=-2^63->LONG_MIN). Reproduced via javaF2L.
//
// Source: 26.1.2/src/net/minecraft/world/entity/AnimationState.java
// Parity enforced by AnimationStateParityTest.cpp vs tools/AnimationStateParity.java.

#pragma once

#include <cstdint>
#include <limits>

namespace mc::world::entity {

// Java float->int narrowing conversion (JLS 5.1.3). C++ (int) on out-of-range
// float is UB, so do it explicitly: NaN->0, clamp to [INT_MIN, INT_MAX], else
// truncate toward zero.
inline int32_t javaF2I(float f) {
    if (f != f) return 0;                                  // NaN
    if (f >= 2147483648.0f) return INT32_MAX;              // >= 2^31
    if (f <= -2147483648.0f) return INT32_MIN;             // <= -2^31 (exactly -2^31 is representable as float -> INT_MIN)
    return static_cast<int32_t>(f);                        // truncate toward zero, in range
}

// Java float->long narrowing conversion (JLS 5.1.3). NaN->0, clamp to
// [LONG_MIN, LONG_MAX], else truncate toward zero.
inline int64_t javaF2L(float f) {
    if (f != f) return 0;                                  // NaN
    if (f >= 9223372036854775808.0f) return INT64_MAX;     // >= 2^63
    if (f <= -9223372036854775808.0f) return INT64_MIN;    // <= -2^63
    return static_cast<int64_t>(f);                        // truncate toward zero, in range
}

class AnimationState {
public:
    static constexpr int32_t STOPPED = INT32_MIN;  // Integer.MIN_VALUE

    // start(int tickCount): this.startTick = tickCount;
    void start(int32_t tickCount) { startTick_ = tickCount; }

    // startIfStopped(int tickCount): if (!isStarted()) start(tickCount);
    void startIfStopped(int32_t tickCount) {
        if (!isStarted()) start(tickCount);
    }

    // animateWhen(boolean condition, int tickCount):
    //   if (condition) startIfStopped(tickCount); else stop();
    void animateWhen(bool condition, int32_t tickCount) {
        if (condition) startIfStopped(tickCount);
        else stop();
    }

    // stop(): this.startTick = Integer.MIN_VALUE;
    void stop() { startTick_ = STOPPED; }

    // fastForward(int ticks, float timeScale):
    //   if (isStarted()) this.startTick -= (int)(ticks * timeScale);
    // (ticks*timeScale) is int*float -> float; narrowing cast saturates (javaF2I);
    // subtraction is Java int (modular two's-complement) -> int32 wraparound.
    void fastForward(int32_t ticks, float timeScale) {
        if (isStarted()) {
            int32_t delta = javaF2I(static_cast<float>(ticks) * timeScale);
            // Java int subtraction wraps two's-complement; do the math in uint32_t.
            startTick_ = static_cast<int32_t>(static_cast<uint32_t>(startTick_) -
                                              static_cast<uint32_t>(delta));
        }
    }

    // getTimeInMillis(float ageInTicks):
    //   float timeInTicks = ageInTicks - this.startTick;
    //   return (long)(timeInTicks * 50.0F);
    // startTick is widened int->float for the subtraction (lossy for large |startTick|).
    int64_t getTimeInMillis(float ageInTicks) const {
        float timeInTicks = ageInTicks - static_cast<float>(startTick_);
        return javaF2L(timeInTicks * 50.0f);
    }

    // isStarted(): return this.startTick != Integer.MIN_VALUE;
    bool isStarted() const { return startTick_ != STOPPED; }

    // copyFrom(AnimationState state): this.startTick = state.startTick;
    void copyFrom(const AnimationState& state) { startTick_ = state.startTick_; }

    // Test-only accessor for the otherwise-private startTick (the real Java class
    // has no getter; the parity tool reflects the field, so we expose it here only
    // so the C++ test can cross-check the raw state). Not part of the Java API.
    int32_t startTickRaw() const { return startTick_; }

private:
    int32_t startTick_ = STOPPED;  // Integer.MIN_VALUE
};

}  // namespace mc::world::entity
