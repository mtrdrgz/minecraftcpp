// WorldBorder.h — bit-exact port of the PURE geometry/lerp math of the Minecraft
// 26.1.2 world border (net.minecraft.world.level.border.WorldBorder and its two
// private BorderExtent implementations).
//
// Source (verbatim, NOT invented):
//   26.1.2/src/net/minecraft/world/level/border/WorldBorder.java
//   26.1.2/src/net/minecraft/world/level/border/BorderStatus.java
//
// WorldBorder extends SavedData and carries a Codec, listeners, datapack TYPE and
// a tick()/update() lifecycle — all of which are world/registry/saved-data coupled
// and are NOT ported here. What IS pure and self-contained is the border BOX math:
// the StaticBorderExtent / MovingBorderExtent getSize / getMinX / getMaxX / getMinZ
// / getMaxZ / getLerpSpeed / getLerpTime / getLerpTarget / getStatus, the
// MovingBorderExtent.calculateSize lerp, and the top-level getDistanceToBorder /
// isWithinBounds / clampVec3ToBound math. Those depend only on plain scalars
// (centerX, centerZ, absoluteMaxSize, size, and the moving-lerp parameters) and so
// are reproduced here EXACTLY, parameterised by those scalars. No listeners,
// no SavedData, no VoxelShape/Shapes (getCollisionShape lives in the engine, not
// this pure helper), no Entity, no tick scheduling.
//
// Bit-exactness contract (mirrors the Java exactly):
//   - WorldBorder.MAX_SIZE is the literal `5.999997E7F` — a FLOAT literal stored in
//     a `double` field, so the value is (double)(float)5.999997e7, NOT the decimal
//     5.999997e7. kMaxSize below preserves that float round-trip.
//   - absoluteMaxSize is an `int` (default 29999984). Mth.clamp's bounds are
//     `-absoluteMaxSize` / `+absoluteMaxSize`, i.e. the int widened to double; we
//     take the int and widen it here exactly as the JVM does.
//   - StaticBorderExtent.updateBox: minX = Mth.clamp(centerX - size/2.0, -max, max).
//     `size / 2.0` is double; the `2.0` literal is verbatim.
//   - MovingBorderExtent.calculateSize: progress = (lerpDuration - lerpProgress) /
//     lerpDuration, where lerpDuration is a DOUBLE (final double lerpDuration =
//     duration) and lerpProgress is a LONG. The subtraction widens the long to
//     double (double - long), then a double divide. progress < 1.0 ? Mth.lerp(
//     progress, from, to) : to.
//   - MovingBorderExtent.getMinX(delta): Mth.clamp(centerX - Mth.lerp(delta,
//     previousSize, size) / 2.0, -max, max). `delta` is a FLOAT (deltaPartialTick);
//     Mth.lerp(float,double,double) resolves to Mth.lerp(double,double,double) (the
//     float widens), so we route through the double lerp.
//   - MovingBorderExtent.getLerpSpeed: Math.abs(from - to) / (lerpEnd - lerpBegin).
//     lerpEnd - lerpBegin is a LONG subtraction (== duration); double / long widens
//     the long. With duration == 0 this is a 0.0/0 -> NaN path in Java; the gate
//     uses duration >= 1 (lerpSizeBetween is only called with from != to and the
//     real engine passes ticks >= 1), matching vanilla usage.
//   - clampVec3ToBound: Mth.clamp(x, getMinX(), getMaxX() - 1.0E-5F). The 1.0E-5F is
//     a FLOAT literal subtracted from a double max -> (double)(float)1.0e-5 widened.
//   - isWithinBounds(x,z,margin): x >= getMinX()-margin && x < getMaxX()+margin && ...
//   - getDistanceToBorder: fromNorth/South/West/East then nested Math.min.
//
// All literals (2.0, 1.0E-5F, 5.999997E7F, 29999984, 2.9999984E7) are taken verbatim
// from the Java.

#ifndef MCPP_WORLD_LEVEL_BORDER_WORLD_BORDER_H
#define MCPP_WORLD_LEVEL_BORDER_WORLD_BORDER_H

#include <cmath>
#include <cstdint>

#include "../levelgen/Mth.h"

namespace mc::world::level::border {

namespace mth = mc::levelgen::mth;

// BorderStatus (BorderStatus.java). The numeric color is the enum constant's field.
enum class BorderStatus : int {
    GROWING = 0,
    SHRINKING = 1,
    STATIONARY = 2,
};

// BorderStatus.getColor() — BorderStatus.java:13-15.
inline int borderStatusColor(BorderStatus s) {
    switch (s) {
        case BorderStatus::GROWING:    return 4259712;
        case BorderStatus::SHRINKING:  return 16724016;
        case BorderStatus::STATIONARY: return 2138367;
    }
    return 0;
}

// WorldBorder.MAX_SIZE = 5.999997E7F (float literal stored in a double field).
inline constexpr double kMaxSize = static_cast<double>(5.999997E7F);
// WorldBorder.MAX_CENTER_COORDINATE = 2.9999984E7 (a plain double literal).
inline constexpr double kMaxCenterCoordinate = 2.9999984E7;
// default WorldBorder.absoluteMaxSize.
inline constexpr int kDefaultAbsoluteMaxSize = 29999984;

// ---------------------------------------------------------------------------
// StaticBorderExtent — WorldBorder.java:494-590 (the pure box math).
// Parameterised by the enclosing border's centerX/centerZ/absoluteMaxSize plus the
// fixed `size`. updateBox computes the four clamped edges; the getters return them.
// ---------------------------------------------------------------------------
struct StaticBorderExtent {
    double centerX;
    double centerZ;
    int absoluteMaxSize;
    double size;

    StaticBorderExtent(double cx, double cz, int absMax, double sz)
        : centerX(cx), centerZ(cz), absoluteMaxSize(absMax), size(sz) {}

    // updateBox() — WorldBorder.java:553-556. Bounds are -absoluteMaxSize /
    // +absoluteMaxSize (int widened to double).
    double getMinX() const {
        return mth::clamp(centerX - size / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }
    double getMinZ() const {
        return mth::clamp(centerZ - size / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }
    double getMaxX() const {
        return mth::clamp(centerX + size / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }
    double getMaxZ() const {
        return mth::clamp(centerZ + size / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }

    double getSize() const { return size; }                  // :528-530
    BorderStatus getStatus() const { return BorderStatus::STATIONARY; } // :533-535
    double getLerpSpeed() const { return 0.0; }              // :538-540
    int64_t getLerpTime() const { return 0L; }               // :543-545
    double getLerpTarget() const { return size; }            // :548-550
};

// ---------------------------------------------------------------------------
// MovingBorderExtent — WorldBorder.java:330-458 (the pure box math).
// Constructed from (from, to, duration, gameTime) plus the enclosing border's
// centerX/centerZ/absoluteMaxSize. lerpProgress / size / previousSize are mutable
// state advanced by update(); the ctor seeds them exactly as the Java does.
// ---------------------------------------------------------------------------
struct MovingBorderExtent {
    double centerX;
    double centerZ;
    int absoluteMaxSize;

    double from;
    double to;
    int64_t lerpEnd;
    int64_t lerpBegin;
    double lerpDuration;     // final double lerpDuration = duration (:343)
    int64_t lerpProgress;    // mutable
    double size;             // mutable
    double previousSize;     // mutable

    // MovingBorderExtent(from, to, duration, gameTime) — :340-350.
    MovingBorderExtent(double cx, double cz, int absMax,
                       double from_, double to_, int64_t duration, int64_t gameTime)
        : centerX(cx), centerZ(cz), absoluteMaxSize(absMax),
          from(from_), to(to_),
          lerpBegin(gameTime),
          lerpDuration(static_cast<double>(duration)),
          lerpProgress(duration) {
        lerpEnd = lerpBegin + duration;
        double s = calculateSize();
        size = s;
        previousSize = s;
    }

    // calculateSize() — :397-400. lerpDuration is double, lerpProgress is long;
    // (double - long) widens the long, then a double divide.
    double calculateSize() const {
        double progress = (lerpDuration - static_cast<double>(lerpProgress)) / lerpDuration;
        return progress < 1.0 ? mth::lerp(progress, from, to) : to;
    }

    double getPreviousSize() const { return previousSize; } // :393-395

    // getMinX/getMaxX/getMinZ/getMaxZ — :352-386. delta is a FLOAT widened to
    // double for Mth.lerp(double,double,double); bounds are int widened to double.
    double getMinX(float deltaPartialTick) const {
        return mth::clamp(centerX - mth::lerp(static_cast<double>(deltaPartialTick),
                                              getPreviousSize(), getSize()) / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }
    double getMinZ(float deltaPartialTick) const {
        return mth::clamp(centerZ - mth::lerp(static_cast<double>(deltaPartialTick),
                                              getPreviousSize(), getSize()) / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }
    double getMaxX(float deltaPartialTick) const {
        return mth::clamp(centerX + mth::lerp(static_cast<double>(deltaPartialTick),
                                              getPreviousSize(), getSize()) / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }
    double getMaxZ(float deltaPartialTick) const {
        return mth::clamp(centerZ + mth::lerp(static_cast<double>(deltaPartialTick),
                                              getPreviousSize(), getSize()) / 2.0,
                          -static_cast<double>(absoluteMaxSize),
                          static_cast<double>(absoluteMaxSize));
    }

    double getSize() const { return size; }   // :388-391

    // getLerpSpeed() — :402-405. Math.abs(from - to) / (lerpEnd - lerpBegin); the
    // denominator is a LONG subtraction (== duration), double / long widens it.
    double getLerpSpeed() const {
        return std::fabs(from - to) / static_cast<double>(lerpEnd - lerpBegin);
    }
    int64_t getLerpTime() const { return lerpProgress; }  // :407-410
    double getLerpTarget() const { return to; }           // :412-415
    // getStatus() — :417-420. to < from ? SHRINKING : GROWING.
    BorderStatus getStatus() const {
        return to < from ? BorderStatus::SHRINKING : BorderStatus::GROWING;
    }

    // update() — :430-441. Decrement lerpProgress, shift previousSize<-size, then
    // recompute size. Returns whether the lerp has finished (lerpProgress <= 0),
    // in which case the engine swaps in a StaticBorderExtent(to). The pure math
    // here advances the moving state; the extent swap is the caller's concern.
    bool update() {
        lerpProgress--;
        previousSize = size;
        size = calculateSize();
        return lerpProgress <= 0L;
    }
};

// ---------------------------------------------------------------------------
// Top-level WorldBorder pure helpers — WorldBorder.java:64-117. These take the
// already-resolved edges (getMinX/getMaxX/getMinZ/getMaxZ from whichever extent)
// so they stay extent-agnostic, exactly as the Java methods do via this.getMinX().
// ---------------------------------------------------------------------------

// getDistanceToBorder(x, z) — WorldBorder.java:104-112.
inline double getDistanceToBorder(double x, double z,
                                  double minX, double maxX, double minZ, double maxZ) {
    double fromNorth = z - minZ;
    double fromSouth = maxZ - z;
    double fromWest = x - minX;
    double fromEast = maxX - x;
    double m = std::min(fromWest, fromEast);
    m = std::min(m, fromNorth);
    return std::min(m, fromSouth);
}

// isWithinBounds(x, z, margin) — WorldBorder.java:72-74.
inline bool isWithinBounds(double x, double z, double margin,
                           double minX, double maxX, double minZ, double maxZ) {
    return x >= minX - margin && x < maxX + margin && z >= minZ - margin && z < maxZ + margin;
}

// isWithinBounds(x, z) — WorldBorder.java:68-70 (margin == 0.0).
inline bool isWithinBounds(double x, double z,
                           double minX, double maxX, double minZ, double maxZ) {
    return isWithinBounds(x, z, 0.0, minX, maxX, minZ, maxZ);
}

// clampVec3ToBound X/Z components — WorldBorder.java:92-94. The upper clamp bound is
// getMaxX() - 1.0E-5F: a FLOAT literal subtracted from a double max.
inline double clampVec3ToBoundX(double x, double minX, double maxX) {
    return mth::clamp(x, minX, maxX - 1.0E-5F);
}
inline double clampVec3ToBoundZ(double z, double minZ, double maxZ) {
    return mth::clamp(z, minZ, maxZ - 1.0E-5F);
}

} // namespace mc::world::level::border

#endif // MCPP_WORLD_LEVEL_BORDER_WORLD_BORDER_H
