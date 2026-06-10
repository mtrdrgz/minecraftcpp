#pragma once

// 1:1 port of net.minecraft.world.food.FoodData (26.1.2).
//
// Stateful hunger model. We port ONLY the world-independent surface:
//   - field defaults (foodLevel=20, saturationLevel=5.0F, exhaustionLevel=0, tickTimer=0)
//   - add(int food, float saturation)        (FoodData.java:19-22, the private eat formula)
//   - addExhaustion(float amount)            (FoodData.java:100-102)
//   - tickWorldIndependent(bool peaceful)    (FoodData.java:35-42, the exhaustion>=... block)
//
// The full tick(ServerPlayer) at FoodData.java:32-72 needs ServerPlayer / ServerLevel /
// Difficulty / GameRules / player.heal / player.isHurt / damage — NOT ported here. Only the
// `if (this.exhaustionLevel > 4.0F) { ... }` block is world-independent; its sole external
// dependency is `difficulty != Difficulty.PEACEFUL`, surfaced here as the `peaceful` flag so
// both branches can be exercised. The natural-regen / starvation tail (lines 44-71) is SKIPPED.
//
// All arithmetic is plain int/float + Mth.clamp; this struct reuses the CERTIFIED engine
// clamp (mc::levelgen::mth::clamp, bit-exact port of net.minecraft.util.Mth.clamp).

#include <algorithm>
#include <cmath>

#include "world/level/levelgen/Mth.h"

namespace mc::world::food {

// Java Math.min(float,float) / Math.max(float,float) per JLS 15.20.2 / java.lang.Math:
// NaN propagates; -0.0 is strictly less than +0.0. These are the EXACT semantics used by
// FoodData.addExhaustion (Math.min) and the tick block (Math.max). std::min/std::max do NOT
// match for NaN / signed-zero ties, so we reproduce Java explicitly.
inline float javaFloatMin(float a, float b) {
    if (a != a) return a;                       // a is NaN -> NaN
    if (b != b) return b;                       // b is NaN -> NaN
    if (a == 0.0f && b == 0.0f) {               // signed-zero tie: -0.0 < +0.0
        // pick the one whose sign bit is set (i.e. -0.0)
        return std::signbit(a) ? a : b;
    }
    return a <= b ? a : b;
}

inline float javaFloatMax(float a, float b) {
    if (a != a) return a;                       // a is NaN -> NaN
    if (b != b) return b;                       // b is NaN -> NaN
    if (a == 0.0f && b == 0.0f) {               // signed-zero tie: +0.0 > -0.0
        return std::signbit(a) ? b : a;
    }
    return a >= b ? a : b;
}

struct FoodData {
    // FoodData.java:14-17
    int   foodLevel       = 20;
    float saturationLevel = 5.0f;
    float exhaustionLevel = 0.0f;
    int   tickTimer       = 0;

    // FoodData.java:19-22 — private void add(int food, float saturation)
    void add(int food, float saturation) {
        this->foodLevel = mc::levelgen::mth::clamp(food + this->foodLevel, 0, 20);
        // NOTE: Java widens the int `this.foodLevel` to float for the clamp max arg.
        this->saturationLevel = mc::levelgen::mth::clamp(
            saturation + this->saturationLevel, 0.0f, static_cast<float>(this->foodLevel));
    }

    // FoodData.java:100-102 — public void addExhaustion(float amount)
    void addExhaustion(float amount) {
        this->exhaustionLevel = javaFloatMin(this->exhaustionLevel + amount, 40.0f);
    }

    // FoodData.java:35-42 — the world-independent portion of tick(ServerPlayer).
    // `peaceful` == (difficulty == Difficulty.PEACEFUL).
    void tickWorldIndependent(bool peaceful) {
        if (this->exhaustionLevel > 4.0f) {
            this->exhaustionLevel -= 4.0f;
            if (this->saturationLevel > 0.0f) {
                this->saturationLevel = javaFloatMax(this->saturationLevel - 1.0f, 0.0f);
            } else if (!peaceful) {
                this->foodLevel = std::max(this->foodLevel - 1, 0);  // Math.max(int,int)
            }
        }
    }
};

}  // namespace mc::world::food
