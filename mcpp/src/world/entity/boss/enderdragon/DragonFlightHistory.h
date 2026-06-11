#pragma once

// 1:1 port of net.minecraft.world.entity.boss.enderdragon.DragonFlightHistory
// (Minecraft 26.1.2). The ender dragon records a rolling 64-entry history of its
// body's y position and y rotation each tick; the renderer reads back a delayed,
// partial-tick-interpolated sample to animate the trailing body segments.
//
// The class is fully self-contained pure math — a fixed-size ring buffer of
// (double y, float yRot) Sample records plus index arithmetic and Mth.lerp /
// Mth.rotLerp interpolation. There is no Level, Entity, RNG, GPU or registry
// coupling, so every member is ported faithfully and bit-exactly. Nothing is
// skipped or stubbed.
//
//   public static final int LENGTH = 64;
//   private static final int MASK = 63;
//   private final Sample[] samples = new Sample[64];
//   private int head = -1;
//
//   DragonFlightHistory()                 — fills all 64 slots with Sample(0.0, 0.0F)
//   copyFrom(DragonFlightHistory)         — System.arraycopy of samples + head
//   record(double y, float yRot)          — push a new sample, advancing head
//   get(int delay)                        — samples[head - delay & 63]
//   get(int delay, float partialTicks)    — Mth.lerp(y) + Mth.rotLerp(yRot) between
//                                           sample(delay+1) and sample(delay)
//
// 1:1 TRAPS reproduced exactly:
//   • `this.samples[this.head - delay & 63]` — Java `&` binds LOOSER than `-`, so the
//     index is (head - delay) & 63. With a possibly-negative (head - delay) this is a
//     bitwise AND on the two's-complement int, NOT a positive modulo. Reproduced with a
//     std::int32_t subtraction (wrapping like Java int) then `& 63`, which yields a value
//     in [0, 63] regardless of sign (low 6 bits of the two's-complement pattern).
//   • record(): `if (head < 0) Arrays.fill(samples, sample);` runs BEFORE the head
//     increment, so the very first record back-fills all 64 slots with that first
//     sample, then `++head` (from -1 to 0) writes it again at index 0. The `== 64`
//     wrap is also reproduced (head can only reach 64 from 63, then resets to 0).
//   • get(delay, partialTicks): the OLD sample is get(delay + 1) and the NEW is
//     get(delay); the lerp/rotLerp alpha is partialTicks with (old -> new) ordering,
//     i.e. Mth.lerp(partialTicks, old.y, new.y) and Mth.rotLerp(partialTicks,
//     old.yRot, new.yRot). yRot interpolation routes through rotLerp -> wrapDegrees
//     (the float shortest-arc blend), NOT a plain lerp.
//
// Mth.lerp(double,double,double) and Mth.rotLerp(float,float,float) come from the
// certified mc::levelgen::mth header. Certified by dragon_flight_history_parity
// (tools/DragonFlightHistoryParity.java vs the REAL net.minecraft class).

#include <array>
#include <cstdint>

#include "../../../level/levelgen/Mth.h"

namespace mc::world::entity::boss::enderdragon {

namespace mth = mc::levelgen::mth;

class DragonFlightHistory {
public:
    // public static final int LENGTH = 64;  (DragonFlightHistory.java:7)
    static constexpr int LENGTH = 64;
    // private static final int MASK = 63;    (DragonFlightHistory.java:8)
    static constexpr int MASK = 63;

    // public record Sample(double y, float yRot) {}  (DragonFlightHistory.java:44-45)
    struct Sample {
        double y = 0.0;
        float yRot = 0.0F;

        constexpr Sample() = default;
        constexpr Sample(double y_, float yRot_) : y(y_), yRot(yRot_) {}

        constexpr bool operator==(const Sample& o) const {
            // record equality: component-wise (Double/Float .equals).
            return y == o.y && yRot == o.yRot;
        }
    };

    // public DragonFlightHistory() { Arrays.fill(this.samples, new Sample(0.0, 0.0F)); }
    // head defaults to -1.  (DragonFlightHistory.java:9-14)
    DragonFlightHistory() {
        samples_.fill(Sample(0.0, 0.0F));
    }

    // public void copyFrom(DragonFlightHistory history) {
    //    System.arraycopy(history.samples, 0, this.samples, 0, 64);
    //    this.head = history.head;
    // }  (DragonFlightHistory.java:16-19)
    void copyFrom(const DragonFlightHistory& history) {
        samples_ = history.samples_;  // arraycopy of all 64 Sample values
        head_ = history.head_;
    }

    // public void record(double y, float yRot) {
    //    Sample sample = new Sample(y, yRot);
    //    if (this.head < 0) { Arrays.fill(this.samples, sample); }
    //    if (++this.head == 64) { this.head = 0; }
    //    this.samples[this.head] = sample;
    // }  (DragonFlightHistory.java:21-32)
    void record(double y, float yRot) {
        Sample sample(y, yRot);
        if (head_ < 0) {
            samples_.fill(sample);
        }
        if (++head_ == 64) {
            head_ = 0;
        }
        samples_[static_cast<std::size_t>(head_)] = sample;
    }

    // public Sample get(int delay) { return this.samples[this.head - delay & 63]; }
    // Java precedence: (head - delay) & 63.  (DragonFlightHistory.java:34-36)
    Sample get(int delay) const {
        const std::int32_t idx = (static_cast<std::int32_t>(head_) - static_cast<std::int32_t>(delay)) & 63;
        return samples_[static_cast<std::size_t>(idx)];
    }

    // public Sample get(int delay, float partialTicks) {
    //    Sample sample = this.get(delay);
    //    Sample sampleOld = this.get(delay + 1);
    //    return new Sample(Mth.lerp(partialTicks, sampleOld.y, sample.y),
    //                      Mth.rotLerp(partialTicks, sampleOld.yRot, sample.yRot));
    // }  (DragonFlightHistory.java:38-42)
    Sample get(int delay, float partialTicks) const {
        Sample sample = get(delay);
        Sample sampleOld = get(delay + 1);
        return Sample(
            mth::lerp(static_cast<double>(partialTicks), sampleOld.y, sample.y),
            mth::rotLerp(partialTicks, sampleOld.yRot, sample.yRot));
    }

    // Exposed only so the parity harness can mirror the REAL head field bit-for-bit.
    int head() const { return head_; }

private:
    // private final Sample[] samples = new Sample[64];  (DragonFlightHistory.java:9)
    std::array<Sample, 64> samples_{};
    // private int head = -1;  (DragonFlightHistory.java:10)
    int head_ = -1;
};

}  // namespace mc::world::entity::boss::enderdragon
