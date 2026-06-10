// 1:1 C++ port of net.minecraft.world.scores.Score — the portable, self-contained
// subset: the stateful int `value` and boolean `locked`.
//
// Source: 26.1.2/src/net/minecraft/world/scores/Score.java
//
//   public class Score implements ReadOnlyScoreInfo {
//      private int value;
//      private boolean locked = true;          // <-- default is true
//      private @Nullable Component display;     // (component-coupled — NOT ported)
//      private @Nullable NumberFormat numberFormat; // (registry-coupled — NOT ported)
//
//      public Score() {}
//      public int value()              { return this.value; }
//      public void value(int score)    { this.value = score; }
//      public boolean isLocked()       { return this.locked; }
//      public void setLocked(bool l)   { this.locked = locked; }
//      ...
//   }
//
// There is NO `add` method on Score — the scoreboard sets the absolute value via
// value(int). `value` is a plain Java int: assignment wraps two's-complement like
// any int, but value(int) does no arithmetic, so the C++ int32_t models it exactly.
//
// NOT ported (component/registry/serialization-coupled — see ScoreParity notes):
//   - display() / display(Component)      (net.minecraft.network.chat.Component)
//   - numberFormat() / numberFormat(...)  (net.minecraft.network.chat.numbers.NumberFormat)
//   - Score(Packed) / pack() / Packed.MAP_CODEC  (codec + Optional<Component/NumberFormat>)
//   - ReadOnlyScoreInfo.formatValue / safeFormatValue  (MutableComponent formatting)

#pragma once

#include <cstdint>

namespace mc::world::scores {

// Mirrors Score's two portable fields and their accessors, verbatim semantics.
class Score {
public:
    // public Score() { }   — value defaults to 0, locked defaults to true.
    Score() = default;

    // @Override public int value() { return this.value; }
    int32_t value() const { return value_; }

    // public void value(final int score) { this.value = score; }
    void value(int32_t score) { value_ = score; }

    // @Override public boolean isLocked() { return this.locked; }
    bool isLocked() const { return locked_; }

    // public void setLocked(final boolean locked) { this.locked = locked; }
    void setLocked(bool locked) { locked_ = locked; }

private:
    int32_t value_ = 0;     // private int value;
    bool locked_ = true;    // private boolean locked = true;
};

}  // namespace mc::world::scores
