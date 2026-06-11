#pragma once

// 1:1 port of net.minecraft.client.renderer.EndFlashState (Minecraft 26.1.2).
//
// PURE / GL-FREE: this class drives the End-dimension "flash" (the periodic sky
// brightening visible from the End). It is entirely deterministic CPU math — it
// touches no GL/GPU/window state, no resources, no GpuBuffer. It owns four float
// fields (intensity, oldIntensity, xAngle, yAngle) plus the flash schedule
// (offset/duration) and a cached seed, and advances them via tick(clockTime).
//
// Every value comes straight from the decompiled Java; nothing is invented:
//   * RandomSource.createThreadLocalInstance(seed) -> the ported
//     SingleThreadedRandomSource (LegacyRandomSource RNG), seeded with
//     clockTime / 600.
//   * Mth.randomBetweenInclusive / Mth.randomBetween are inlined exactly as in
//     Mth.java:664-669 (random.nextInt(maxInclusive-min+1)+min ;
//     random.nextFloat()*(maxExclusive-min)+min).
//   * Mth.sin(float) uses the certified table-based mc::levelgen::mth::sin (NOT
//     std::sin), and Mth.lerp(float,float,float) uses mc::levelgen::mth::lerpF.
//
// 1:1 traps reproduced here:
//   - calculateFlashParameters first burns ONE random.nextFloat() (Java line 30:
//     "randomSource.nextFloat();") BEFORE the four randomBetween* draws — the RNG
//     stream order is load-bearing. Skipping it desyncs every later value.
//   - Mth.randomBetweenInclusive bound = Math.min(380, 600-offset) for duration's
//     maxInclusive, computed AFTER offset is drawn (so the two draws are ordered).
//   - calculateIntensity uses the table sin of
//     (float)(t-offset) * (float)Math.PI / duration ; the (float)Math.PI literal
//     is Mth.PI = (float)Math.PI, and the divide is float / int(promoted).
//   - flashSeed/offset/duration/angles only recompute when clockTime/600 crosses a
//     new bucket; otherwise the cached schedule is reused (so calling tick twice in
//     the same 600-tick window does NOT redraw — only intensity advances).
//   - oldIntensity is the PREVIOUS tick's intensity (set before recomputing).

#include <algorithm>
#include <cstdint>
#include <memory>

#include "world/level/levelgen/Mth.h"
#include "world/level/levelgen/RandomSource.h"

namespace mc::client::renderer {

class EndFlashState {
public:
    // Java constants (EndFlashState.java:7-11).
    static constexpr std::int32_t SOUND_DELAY_IN_TICKS = 30;
    static constexpr std::int32_t FLASH_INTERVAL_IN_TICKS = 600;
    static constexpr std::int32_t MAX_FLASH_OFFSET_IN_TICKS = 200;
    static constexpr std::int32_t MIN_FLASH_DURATION_IN_TICKS = 100;
    static constexpr std::int32_t MAX_FLASH_DURATION_IN_TICKS = 380;

    // Java: public void tick(final long clockTime)
    void tick(std::int64_t clockTime) {
        calculateFlashParameters(clockTime);
        m_oldIntensity = m_intensity;
        m_intensity = calculateIntensity(clockTime);
    }

    // Java: public float getXAngle()
    float getXAngle() const { return m_xAngle; }

    // Java: public float getYAngle()
    float getYAngle() const { return m_yAngle; }

    // Java: public float getIntensity(final float partialTicks)
    float getIntensity(float partialTicks) const {
        return mc::levelgen::mth::lerpF(partialTicks, m_oldIntensity, m_intensity);
    }

    // Java: public boolean flashStartedThisTick()
    bool flashStartedThisTick() const {
        return m_intensity > 0.0F && m_oldIntensity <= 0.0F;
    }

    // Exposed for parity verification (mirror Java's private fields).
    std::int64_t flashSeedValue() const { return m_flashSeed; }
    std::int32_t offsetValue() const { return m_offset; }
    std::int32_t durationValue() const { return m_duration; }
    float intensityValue() const { return m_intensity; }
    float oldIntensityValue() const { return m_oldIntensity; }

private:
    // Mth.java:664-666 — randomBetweenInclusive(random, min, maxInclusive).
    static std::int32_t randomBetweenInclusive(mc::levelgen::RandomSource& random,
                                               std::int32_t min, std::int32_t maxInclusive) {
        return random.nextInt(maxInclusive - min + 1) + min;
    }

    // Mth.java:668-670 — randomBetween(random, min, maxExclusive).
    static float randomBetween(mc::levelgen::RandomSource& random,
                               float min, float maxExclusive) {
        return random.nextFloat() * (maxExclusive - min) + min;
    }

    // Java: private void calculateFlashParameters(final long clockTime)
    void calculateFlashParameters(std::int64_t clockTime) {
        std::int64_t newSeed = clockTime / 600LL;
        if (newSeed != m_flashSeed) {
            std::shared_ptr<mc::levelgen::RandomSource> randomSource =
                mc::levelgen::RandomSource::createThreadLocalInstance(newSeed);
            randomSource->nextFloat();  // Java line 30 — burned draw, order matters.
            m_offset = randomBetweenInclusive(*randomSource, 0, 200);
            m_duration = randomBetweenInclusive(*randomSource, 100,
                                                std::min(380, 600 - m_offset));
            m_xAngle = randomBetween(*randomSource, -60.0F, 10.0F);
            m_yAngle = randomBetween(*randomSource, -180.0F, 180.0F);
            m_flashSeed = newSeed;
        }
    }

    // Java: private float calculateIntensity(final long clockTime)
    float calculateIntensity(std::int64_t clockTime) const {
        std::int64_t clockTimeWithinInterval = clockTime % 600LL;
        if (clockTimeWithinInterval >= m_offset
            && clockTimeWithinInterval <= static_cast<std::int64_t>(m_offset) + m_duration) {
            // Mth.PI = (float)Math.PI; Math.PI is the IEEE-754 double nearest to pi.
            // Mth.sin uses the certified table-based approximation (NOT std::sin).
            const float MTH_PI = static_cast<float>(3.141592653589793);  // == (float)Math.PI
            return mc::levelgen::mth::sin(
                static_cast<float>(clockTimeWithinInterval - m_offset) * MTH_PI / m_duration);
        }
        return 0.0F;
    }

    // Java initial field values (defaults): flashSeed=0, offset=0, duration=0,
    // intensity=0, oldIntensity=0, xAngle=0, yAngle=0.
    std::int64_t m_flashSeed = 0;
    std::int32_t m_offset = 0;
    std::int32_t m_duration = 0;
    float m_intensity = 0.0F;
    float m_oldIntensity = 0.0F;
    float m_xAngle = 0.0F;
    float m_yAngle = 0.0F;
};

}  // namespace mc::client::renderer
