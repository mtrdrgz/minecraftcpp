// Bit-exact 1:1 C++ port of net.minecraft.client.renderer.RunningTrimmedMean
// (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/client/renderer/RunningTrimmedMean.java
//
// A fixed-capacity ring buffer of `long` samples. registerValueAndGetMean(v)
// stores v, then returns the "trimmed mean" of the currently-held samples: when
// more than two samples are present it discards exactly one min and one max and
// averages the rest; otherwise it falls back to a peculiar branch (see below).
// Used by the renderer to smooth frame-time / timing measurements — pure 64-bit
// integer arithmetic, no GL/GPU/window state whatsoever.
//
// ── 1:1 traps faithfully reproduced ─────────────────────────────────────────
//   * The (count <= 2) fallback is `total > 0 ? count / total : 0` — that is
//     `count` (the int sample count) divided by `total` (sum of longs), NOT the
//     intuitive `total / count`. This near-always yields 0 (since total >= count
//     for positive samples) and is exactly what the real class does; we keep it.
//   * Java integer division truncates toward zero. For the (count > 2) branch
//     `total / (count - 2)` `total` can be negative (negative samples), so the
//     result truncates toward zero just like Java `long /`. C++ `/` on int64_t
//     matches this verbatim.
//   * min/max seeded with Long.MAX_VALUE / Long.MIN_VALUE; Math.min/max on longs.
//   * cursor advance is `(cursor + 1) % values.length` (length == maxCount).
//   * count saturates at values.length and only ever increments.
//   * The loop sums values[0 .. count) — i.e. the first `count` ring slots in
//     index order, NOT chronological order (matters once the ring wraps).
//
// Verbatim Java reference:
//
//   public long registerValueAndGetMean(final long value) {
//      if (this.count < this.values.length) { this.count++; }
//      this.values[this.cursor] = value;
//      this.cursor = (this.cursor + 1) % this.values.length;
//      long min = Long.MAX_VALUE;
//      long max = Long.MIN_VALUE;
//      long total = 0L;
//      for (int i = 0; i < this.count; i++) {
//         long current = this.values[i];
//         total += current;
//         min = Math.min(min, current);
//         max = Math.max(max, current);
//      }
//      if (this.count > 2) {
//         total -= min + max;
//         return total / (this.count - 2);
//      } else {
//         return total > 0L ? this.count / total : 0L;
//      }
//   }

#pragma once

#include <cstdint>
#include <limits>
#include <vector>

namespace mc::render {

class RunningTrimmedMean {
public:
    // Mirrors `new RunningTrimmedMean(maxCount)`: allocates the backing array
    // of `maxCount` longs, zero-initialized (Java `new long[maxCount]`).
    explicit RunningTrimmedMean(int32_t maxCount)
        : values_(static_cast<size_t>(maxCount), 0LL) {}

    // long registerValueAndGetMean(long value)
    int64_t registerValueAndGetMean(int64_t value) {
        const int32_t length = static_cast<int32_t>(values_.size());

        if (count_ < length) {
            count_++;
        }

        values_[static_cast<size_t>(cursor_)] = value;
        cursor_ = (cursor_ + 1) % length;

        int64_t min = std::numeric_limits<int64_t>::max();   // Long.MAX_VALUE
        int64_t max = std::numeric_limits<int64_t>::min();   // Long.MIN_VALUE
        int64_t total = 0LL;

        for (int32_t i = 0; i < count_; i++) {
            int64_t current = values_[static_cast<size_t>(i)];
            total += current;                                 // Java long wrap is 2's-complement
            if (current < min) min = current;                 // Math.min
            if (current > max) max = current;                 // Math.max
        }

        if (count_ > 2) {
            total -= min + max;
            return total / static_cast<int64_t>(count_ - 2);  // long / long, trunc toward 0
        } else {
            // NOTE: this is `count / total`, not `total / count` — see header.
            return total > 0LL ? static_cast<int64_t>(count_) / total : 0LL;
        }
    }

private:
    std::vector<int64_t> values_;
    int32_t count_ = 0;
    int32_t cursor_ = 0;
};

}  // namespace mc::render
