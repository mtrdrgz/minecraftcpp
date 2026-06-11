// 1:1 port of the PURE state-machine subset of
// net.minecraft.world.effect.MobEffectInstance (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/effect/MobEffectInstance.java
//
// This header ports ONLY the parts of MobEffectInstance that are pure
// integer/float logic over the instance's own fields (duration, amplifier,
// ambient, visible, showIcon) plus the hidden-effect linked-list chain. The
// `effect` Holder<MobEffect> is modelled here as an opaque identity key
// (`effectId`) because every pure method below touches the effect only through
// `this.effect.equals(takeOver.effect)` — never its value(). That is exactly the
// Java semantics: Holder equality is identity/key equality.
//
// PORTED (all 1:1 with the Java line refs):
//   * constructor amplifier clamp  Mth.clamp(amplifier,0,255)        (.java:81)
//   * setDetailsFrom                                                  (.java:124-130)
//   * update(takeOver)              hidden-effect promotion machine   (.java:132-175)
//   * isShorterDurationThan                                           (.java:177-179)
//   * isInfiniteDuration                                              (.java:181-183)
//   * endsWithin                                                      (.java:185-187)
//   * withScaledDuration            Math.max(Mth.floor(d*scale),1)    (.java:189-193)
//   * mapDuration                                                     (.java:195-197)
//   * hasRemainingDuration                                            (.java:251-253)
//   * tickDownDuration             recursive over hidden chain        (.java:255-261)
//   * downgradeToHiddenEffect                                         (.java:263-271)
//   * hashCode                      31*… mixing, two's-complement wrap (.java:330-337)
//
// NOT PORTED here (registry / world / GL coupled — hard absent, never faked):
//   * getBlendFactor / BlendState  — needs MobEffect.getBlendIn/OutDurationTicks
//     and a live LivingEntity. (.java:116-118, 370-411)
//   * tickServer / tickClient / onEffect* / onMob*  — ServerLevel + LivingEntity.
//   * compareTo                     — needs MobEffect.getColor() from the registry
//     (.java:339-352).
//   * getParticleOptions / toString / Codec / StreamCodec / Details record.
//   * getEffect()                   — returns the Holder; here only the id is kept.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace mc {

// Local 1:1 copies of the only two Mth helpers this class needs, kept inline so
// the header is self-contained (they match mcpp/src/world/level/levelgen/Mth.h
// and mcpp/src/world/phys/shapes/JavaMath.h):
//   Mth.clamp(int,int,int)  = Math.min(Math.max(value,min),max)   (Mth.java:93-95)
//   Mth.floor(float)        = (int)Math.floor((double)v)          (Mth.java:61-63)
// Math.floor takes a double, so the float arg widens to double first — this
// matters for values not exactly representable in float.
namespace mei_detail {
inline int32_t clampInt(int32_t value, int32_t min, int32_t max) {
    return std::min(std::max(value, min), max);
}
// Java narrowing double->int (JLS 5.1.3): NaN -> 0, saturates at the int range,
// otherwise truncates toward zero. A raw C++ static_cast of an out-of-range
// double to int32_t is UNDEFINED BEHAVIOUR (and the optimizer produced a wrong
// value here), so we replicate Java's saturating cast exactly. This is the
// load-bearing trap in withScaledDuration when duration*scale overflows int.
inline int32_t jintCast(double v) {
    if (std::isnan(v)) return 0;
    if (v >= 2147483647.0) return INT32_MAX;
    if (v <= -2147483648.0) return INT32_MIN;
    return static_cast<int32_t>(v);
}
inline int32_t floorFloat(float v) {
    return jintCast(std::floor(static_cast<double>(v)));
}
} // namespace mei_detail

class MobEffectInstance {
public:
    // MobEffectInstance.java:27-29.
    static constexpr int32_t INFINITE_DURATION = -1;
    static constexpr int32_t MIN_AMPLIFIER = 0;
    static constexpr int32_t MAX_AMPLIFIER = 255;

    // The full canonical constructor (.java:70-86). `effectId` stands in for the
    // Holder<MobEffect> identity. hiddenEffect is an owned chain (nullable).
    MobEffectInstance(int64_t effectId, int32_t duration, int32_t amplifier,
                      bool ambient, bool visible, bool showIcon,
                      std::unique_ptr<MobEffectInstance> hiddenEffect = nullptr)
        : effect_(effectId),
          duration_(duration),
          // .java:81 — Mth.clamp(amplifier, 0, 255).
          amplifier_(mei_detail::clampInt(amplifier, MIN_AMPLIFIER, MAX_AMPLIFIER)),
          ambient_(ambient),
          visible_(visible),
          showIcon_(showIcon),
          hiddenEffect_(std::move(hiddenEffect)) {}

    // Copy constructor (.java:88-91): same effect, details copied via
    // setDetailsFrom (note: hiddenEffect is NOT copied here — matches Java).
    MobEffectInstance(const MobEffectInstance& copy)
        : effect_(copy.effect_) {
        setDetailsFrom(copy);
    }

    // .java:124-130 — copies the five mutable detail fields only.
    void setDetailsFrom(const MobEffectInstance& copy) {
        duration_ = copy.duration_;
        amplifier_ = copy.amplifier_;
        ambient_ = copy.ambient_;
        visible_ = copy.visible_;
        showIcon_ = copy.showIcon_;
    }

    // .java:132-175 — the take-over / hidden-effect promotion state machine.
    // (We drop the LOGGER.warn on effect mismatch — it has no state effect.)
    bool update(const MobEffectInstance& takeOver) {
        bool changed = false;
        if (takeOver.amplifier_ > amplifier_) {
            if (takeOver.isShorterDurationThan(*this)) {
                // this.hiddenEffect = new MobEffectInstance(this); then re-link
                // the previous hidden chain onto it (.java:140-142).
                std::unique_ptr<MobEffectInstance> prevHidden = std::move(hiddenEffect_);
                hiddenEffect_ = std::make_unique<MobEffectInstance>(*this);
                hiddenEffect_->hiddenEffect_ = std::move(prevHidden);
            }
            amplifier_ = takeOver.amplifier_;
            duration_ = takeOver.duration_;
            changed = true;
        } else if (isShorterDurationThan(takeOver)) {
            if (takeOver.amplifier_ == amplifier_) {
                duration_ = takeOver.duration_;
                changed = true;
            } else if (hiddenEffect_ == nullptr) {
                hiddenEffect_ = std::make_unique<MobEffectInstance>(takeOver);
            } else {
                hiddenEffect_->update(takeOver);
            }
        }

        // .java:159-162 — note the short-circuit AND vs OR exactly.
        if ((!takeOver.ambient_ && ambient_) || changed) {
            ambient_ = takeOver.ambient_;
            changed = true;
        }
        if (takeOver.visible_ != visible_) {
            visible_ = takeOver.visible_;
            changed = true;
        }
        if (takeOver.showIcon_ != showIcon_) {
            showIcon_ = takeOver.showIcon_;
            changed = true;
        }
        return changed;
    }

    // .java:177-179.
    bool isShorterDurationThan(const MobEffectInstance& other) const {
        return !isInfiniteDuration() &&
               (duration_ < other.duration_ || other.isInfiniteDuration());
    }

    // .java:181-183.
    bool isInfiniteDuration() const { return duration_ == INFINITE_DURATION; }

    // .java:185-187.
    bool endsWithin(int32_t ticks) const {
        return !isInfiniteDuration() && duration_ <= ticks;
    }

    // .java:189-193 — withScaledDuration. Copies the instance, then sets
    //   copy.duration = copy.mapDuration(d -> Math.max(Mth.floor(d * scale), 1)).
    // Mth.floor((float)(int d) * (float)scale) is an int->float widen, float
    // multiply, then (int)Math.floor (truncate-toward-negative-infinity).
    MobEffectInstance withScaledDuration(float scale) const {
        MobEffectInstance copy(*this);
        copy.duration_ = copy.mapDuration([scale](int32_t d) {
            return std::max(mei_detail::floorFloat(static_cast<float>(d) * scale), 1);
        });
        return copy;
    }

    // .java:195-197 — only remap finite, non-zero durations; otherwise identity.
    template <class Mapper>
    int32_t mapDuration(Mapper mapper) const {
        return (!isInfiniteDuration() && duration_ != 0) ? mapper(duration_)
                                                         : duration_;
    }

    // .java:251-253.
    bool hasRemainingDuration() const {
        return isInfiniteDuration() || duration_ > 0;
    }

    // .java:255-261 — recurse into the hidden chain FIRST, then tick self.
    void tickDownDuration() {
        if (hiddenEffect_ != nullptr) {
            hiddenEffect_->tickDownDuration();
        }
        duration_ = mapDuration([](int32_t d) {
            // Java int subtraction; wrap in unsigned to avoid C++ signed-overflow
            // UB at INT_MIN (mapDuration already excludes d==0/-1 from here).
            return static_cast<int32_t>(static_cast<uint32_t>(d) - 1u);
        });
    }

    // .java:263-271 — when self hits 0 and a hidden effect waits, promote it.
    bool downgradeToHiddenEffect() {
        if (duration_ == 0 && hiddenEffect_ != nullptr) {
            setDetailsFrom(*hiddenEffect_);
            // this.hiddenEffect = this.hiddenEffect.hiddenEffect;
            hiddenEffect_ = std::move(hiddenEffect_->hiddenEffect_);
            return true;
        }
        return false;
    }

    // .java:330-337 — note: hashCode does NOT depend on hiddenEffect, and uses
    // 31 * acc + field with two's-complement int wrap. effect.hashCode() is the
    // seed; here it is the identity-key hash (caller supplies the same seed as
    // the Java Holder.hashCode for that effect — see the GT, which prints the
    // real Holder.hashCode as the effectId/seed).
    int32_t hashCode(int32_t effectHash) const {
        uint32_t result = static_cast<uint32_t>(effectHash);
        result = 31u * result + static_cast<uint32_t>(duration_);
        result = 31u * result + static_cast<uint32_t>(amplifier_);
        result = 31u * result + static_cast<uint32_t>(ambient_ ? 1 : 0);
        result = 31u * result + static_cast<uint32_t>(visible_ ? 1 : 0);
        result = 31u * result + static_cast<uint32_t>(showIcon_ ? 1 : 0);
        return static_cast<int32_t>(result);
    }

    int64_t effect() const { return effect_; }
    int32_t getDuration() const { return duration_; }
    int32_t getAmplifier() const { return amplifier_; }
    bool isAmbient() const { return ambient_; }
    bool isVisible() const { return visible_; }
    bool showIcon() const { return showIcon_; }
    const MobEffectInstance* hidden() const { return hiddenEffect_.get(); }

private:
    int64_t effect_;
    int32_t duration_;
    int32_t amplifier_;
    bool ambient_;
    bool visible_;
    bool showIcon_;
    std::unique_ptr<MobEffectInstance> hiddenEffect_;
};

} // namespace mc
