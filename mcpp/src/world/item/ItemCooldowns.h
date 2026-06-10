#pragma once

// 1:1 port of net.minecraft.world.item.ItemCooldowns (26.1.2).
//
// The vanilla class keys cooldowns by `Identifier` (the item's cooldown group),
// resolved from the live Item registry / UseCooldown data component. That
// resolution (getCooldownGroup, addCooldown(ItemStack), isOnCooldown(ItemStack))
// is registry/component-coupled and is intentionally NOT ported here — see the
// unported list in the parity report. Everything below operates on the *resolved*
// group, which vanilla represents as an Identifier; we represent it as a raw
// integer group id (a distinct id per distinct Identifier). The stateful tick
// machinery — the cooldown map, tickCount, expiry sweep, and the
// duration/remaining percentage math — is reproduced verbatim.
//
// Source (ItemCooldowns.java):
//   getCooldownPercent : duration = endTime-startTime;
//                        remaining = endTime-(tickCount+partialTick);
//                        return Mth.clamp(remaining/duration, 0.0F, 1.0F);   (:21-31)
//   isOnCooldown       : getCooldownPercent(item, 0.0F) > 0.0F               (:17-19)
//   tick               : tickCount++; sweep entries with endTime <= tickCount(:33-46)
//   addCooldown(group) : cooldowns.put(group, {tickCount, tickCount+time})  (:58-61)
//   removeCooldown     : cooldowns.remove(group)                            (:63-66)
//
// Mth.clamp(float,float,float) (Mth.java:101-103) = `value<min?min:Math.min(value,max)`.
// With finite/physical inputs Math.min == std::min, so we inline that body here
// rather than pulling in the worldgen Mth header.

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace mc::world::item {

class ItemCooldowns {
public:
    // CooldownInstance(int startTime, int endTime) — ItemCooldowns.java:74.
    struct CooldownInstance {
        std::int32_t startTime;
        std::int32_t endTime;
    };

    // addCooldown(Identifier cooldownGroup, int time) — ItemCooldowns.java:58-61.
    // (onCooldownStarted is an empty protected hook; no-op here.)
    void addCooldown(std::int32_t cooldownGroup, std::int32_t time) {
        cooldowns_[cooldownGroup] =
            CooldownInstance{tickCount_, static_cast<std::int32_t>(tickCount_ + time)};
        onCooldownStarted(cooldownGroup, time);
    }

    // removeCooldown(Identifier cooldownGroup) — ItemCooldowns.java:63-66.
    void removeCooldown(std::int32_t cooldownGroup) {
        cooldowns_.erase(cooldownGroup);
        onCooldownEnded(cooldownGroup);
    }

    // getCooldownPercent(group, partialTick) — the body of ItemCooldowns.java:21-31,
    // operating on the already-resolved group instead of an ItemStack.
    float getCooldownPercent(std::int32_t cooldownGroup, float partialTick) const {
        auto it = cooldowns_.find(cooldownGroup);
        if (it != cooldowns_.end()) {
            const CooldownInstance& cooldown = it->second;
            // Java float arithmetic: int->float widening is exact for these ranges,
            // the subtractions/division are IEEE-754 single precision.
            float duration  = static_cast<float>(cooldown.endTime)
                            - static_cast<float>(cooldown.startTime);
            float remaining = static_cast<float>(cooldown.endTime)
                            - (static_cast<float>(tickCount_) + partialTick);
            return clamp(remaining / duration, 0.0F, 1.0F);
        }
        return 0.0F;
    }

    // isOnCooldown(item) — ItemCooldowns.java:17-19, on the resolved group.
    bool isOnCooldown(std::int32_t cooldownGroup) const {
        return getCooldownPercent(cooldownGroup, 0.0F) > 0.0F;
    }

    // tick() — ItemCooldowns.java:33-46. tickCount++ then drop expired entries
    // (endTime <= tickCount). Removal order does not affect the result.
    void tick() {
        ++tickCount_;
        for (auto it = cooldowns_.begin(); it != cooldowns_.end();) {
            if (it->second.endTime <= tickCount_) {
                std::int32_t key = it->first;
                it = cooldowns_.erase(it);
                onCooldownEnded(key);
            } else {
                ++it;
            }
        }
    }

    std::int32_t tickCount() const { return tickCount_; }

protected:
    // Empty hooks in vanilla (ItemCooldowns.java:68-72).
    virtual void onCooldownStarted(std::int32_t /*cooldownGroup*/, std::int32_t /*duration*/) {}
    virtual void onCooldownEnded(std::int32_t /*cooldownGroup*/) {}

private:
    // Mth.clamp(float,float,float) — Mth.java:101-103.
    static float clamp(float value, float min, float max) {
        return value < min ? min : std::min(value, max);
    }

    std::unordered_map<std::int32_t, CooldownInstance> cooldowns_;
    std::int32_t tickCount_ = 0;
};

}  // namespace mc::world::item
