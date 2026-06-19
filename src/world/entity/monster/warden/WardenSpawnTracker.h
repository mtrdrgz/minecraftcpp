// 1:1 port of the pure, self-contained integer state machine of
//   net.minecraft.world.entity.monster.warden.WardenSpawnTracker (Minecraft 26.1.2).
//
// WardenSpawnTracker tracks, per-player, how close the Warden is to spawning.
// Its STATIC entry points (tryWarn) are world-coupled (they search a ServerLevel
// for nearby wardens/players), and are NOT ported here. But the object itself is
// nothing more than three ints
//
//   { ticksSinceLastWarning, warningLevel, cooldownTicks }
//
// and the methods that evolve that state read *no* world/entity/registry data:
//
//   WardenSpawnTracker.java (26.1.2):
//     public void tick() {
//        if (this.ticksSinceLastWarning >= 12000) {      // DECREASE_..._INTERVAL
//           this.decreaseWarningLevel();
//           this.ticksSinceLastWarning = 0;
//        } else {
//           this.ticksSinceLastWarning++;
//        }
//        if (this.cooldownTicks > 0) {
//           this.cooldownTicks--;
//        }
//     }
//     private void increaseWarningLevel() {
//        if (!this.onCooldown()) {
//           this.ticksSinceLastWarning = 0;
//           this.cooldownTicks = 200;                     // WARNING_LEVEL_INCREASE_COOLDOWN
//           this.setWarningLevel(this.getWarningLevel() + 1);
//        }
//     }
//     private void decreaseWarningLevel() {
//        this.setWarningLevel(this.getWarningLevel() - 1);
//     }
//     public void setWarningLevel(final int warningLevel) {
//        this.warningLevel = Mth.clamp(warningLevel, 0, 4); // MAX_WARNING_LEVEL = 4
//     }
//     private boolean onCooldown() { return this.cooldownTicks > 0; }
//     public void reset() { all three -> 0; }
//
// This is a tight bundle of 1:1 traps:
//   * tick() is a `>=` threshold branch: at exactly 12000 it RESETS the counter
//     to 0 AND decreases the warning level; for any value < 12000 it post-increments.
//     An off-by-one (`>` vs `>=`) or resetting at the wrong edge diverges.
//   * the cooldown gate in increaseWarningLevel(): when cooldownTicks > 0 the call
//     is a no-op (level unchanged), otherwise it zeroes ticksSinceLastWarning,
//     arms cooldown to exactly 200, and bumps the (clamped) level. Dropping the
//     gate would let the level run away — the exact gating vanilla relies on.
//   * setWarningLevel clamps to [0, 4] via Mth.clamp == min(max(v,0),4). Decrease
//     below 0 clamps to 0; increase past 4 clamps to 4 (so a 5th increase, even
//     off cooldown, still leaves level == 4 but DOES reset ticks + re-arm cooldown).
//   * cooldownTicks only decrements while > 0 (no underflow into negatives).
//
// Constants and method bodies are copied verbatim from the source; nothing is
// invented. Verified bit-for-bit against the REAL class (driven via reflection on
// a real instance) by warden_spawn_tracker_parity (tools/WardenSpawnTrackerParity.java).
#pragma once

namespace mc::world::entity::monster::warden {

// WardenSpawnTracker.java (26.1.2) constant block.
inline constexpr int MAX_WARNING_LEVEL = 4;                       // public static final int
inline constexpr int DECREASE_WARNING_LEVEL_EVERY_INTERVAL = 12000; // private static final int
inline constexpr int WARNING_LEVEL_INCREASE_COOLDOWN = 200;       // private static final int

// Mth.clamp(int,int,int) == Math.min(Math.max(value, min), max). (Mth.java:93-95)
inline int mthClampInt(int value, int lo, int hi) {
    int a = value > lo ? value : lo; // Math.max(value, min)
    return a < hi ? a : hi;          // Math.min(., max)
}

// A faithful copy of the WardenSpawnTracker instance: exactly the three ints and
// the pure methods that evolve them. Field order and defaults match the source.
struct WardenSpawnTracker {
    int ticksSinceLastWarning = 0;
    int warningLevel = 0;
    int cooldownTicks = 0;

    WardenSpawnTracker() = default;
    WardenSpawnTracker(int ticksSinceLastWarning_, int warningLevel_, int cooldownTicks_)
        : ticksSinceLastWarning(ticksSinceLastWarning_),
          warningLevel(warningLevel_),
          cooldownTicks(cooldownTicks_) {}

    // private boolean onCooldown() { return this.cooldownTicks > 0; }
    bool onCooldown() const { return cooldownTicks > 0; }

    int getWarningLevel() const { return warningLevel; }

    // public void setWarningLevel(final int warningLevel) {
    //    this.warningLevel = Mth.clamp(warningLevel, 0, 4);
    // }
    void setWarningLevel(int level) {
        warningLevel = mthClampInt(level, 0, MAX_WARNING_LEVEL);
    }

    // private void decreaseWarningLevel() {
    //    this.setWarningLevel(this.getWarningLevel() - 1);
    // }
    void decreaseWarningLevel() {
        setWarningLevel(getWarningLevel() - 1);
    }

    // private void increaseWarningLevel() {
    //    if (!this.onCooldown()) {
    //       this.ticksSinceLastWarning = 0;
    //       this.cooldownTicks = 200;
    //       this.setWarningLevel(this.getWarningLevel() + 1);
    //    }
    // }
    void increaseWarningLevel() {
        if (!onCooldown()) {
            ticksSinceLastWarning = 0;
            cooldownTicks = WARNING_LEVEL_INCREASE_COOLDOWN;
            setWarningLevel(getWarningLevel() + 1);
        }
    }

    // public void tick() {
    //    if (this.ticksSinceLastWarning >= 12000) {
    //       this.decreaseWarningLevel();
    //       this.ticksSinceLastWarning = 0;
    //    } else {
    //       this.ticksSinceLastWarning++;
    //    }
    //    if (this.cooldownTicks > 0) {
    //       this.cooldownTicks--;
    //    }
    // }
    void tick() {
        if (ticksSinceLastWarning >= DECREASE_WARNING_LEVEL_EVERY_INTERVAL) {
            decreaseWarningLevel();
            ticksSinceLastWarning = 0;
        } else {
            ticksSinceLastWarning++;
        }
        if (cooldownTicks > 0) {
            cooldownTicks--;
        }
    }

    // public void reset() { ticksSinceLastWarning = warningLevel = cooldownTicks = 0; }
    void reset() {
        ticksSinceLastWarning = 0;
        warningLevel = 0;
        cooldownTicks = 0;
    }
};

} // namespace mc::world::entity::monster::warden
