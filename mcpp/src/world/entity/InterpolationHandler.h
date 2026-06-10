#pragma once

// 1:1 port of net.minecraft.world.entity.InterpolationHandler (26.1.2) — the
// helper that smooths an entity's position/rotation toward a network-supplied
// target over a fixed number of ticks (DEFAULT_INTERPOLATION_STEPS = 3), used by
// vehicles / display entities / interpolated mobs.
//
// The real class is coupled to a live Entity + Level (it reads
// entity.position()/getYRot()/getXRot(), calls entity.level().noCollision(...)
// against entity.makeBoundingBox(...), and writes back via entity.setPos/setRot/
// snapTo). That world/collision surface CANNOT be ported here without a live
// level, so it is NOT reproduced. What IS ported, faithfully and bit-exactly, is:
//
//   • InterpolationData (steps, position, yRot, xRot) and its mutators
//       decrease()            — InterpolationHandler.java:128-130
//       addDelta(Vec3)        — InterpolationHandler.java:132-134
//       addRotation(yRot,xRot)— InterpolationHandler.java:136-139
//   • the per-tick replay arithmetic of interpolate() — InterpolationHandler.java:77-107 —
//     expressed as a pure function of the entity's OBSERVABLE per-tick state
//     (its current position/yRot/xRot, the noCollision boolean for this tick, and
//     the previousTick position/rotation). This is the "position/rotation
//     interpolation steps over ticks (Vec3 lerp); replay sequence" requested.
//
// SKIPPED (live Entity / Level coupling — absent here, listed in unportedMethods,
// NOT stubbed to a no-op):
//   • interpolateTo(...)  — needs entity.position()/getYRot()/getXRot()/snapTo + onInterpolationStart callback
//   • the noCollision / makeBoundingBox guard inside interpolate() — needs Level+AABB
//   • position()/yRot()/xRot()/hasActiveInterpolation()/cancel() trivial accessors
//     over the same coupled state (cancel()'s field resets are reproduced inside step())
//
// Math: Mth.lerp(double,double,double) and Mth.rotLerp(double,double,double) come
// from the certified mc::levelgen::mth header (rotLerp uses wrapDegrees(double));
// Vec3.subtract/add from the certified Vec3. The (float) narrowing of the double
// rotLerp/lerp results matches Java's (float) cast exactly (round-to-nearest-even).
//
// Certified by interpolation_handler_parity (tools/InterpolationHandlerParity.java).

#include "../phys/Vec2.h"
#include "../phys/Vec3.h"
#include "../level/levelgen/Mth.h"

namespace mc {

namespace mth = mc::levelgen::mth;

// net.minecraft.world.entity.InterpolationHandler.InterpolationData (private static
// inner class, InterpolationHandler.java:115-140). Plain mutable record of the
// in-flight interpolation target.
struct InterpolationData {
    int   steps;       // remaining ticks
    Vec3  position;    // target position (mutated by addDelta as the entity drifts)
    float yRot;        // target yaw   (mutated by addRotation)
    float xRot;        // target pitch (mutated by addRotation)

    // private InterpolationData(int, Vec3, float, float) — :121-126.
    InterpolationData(int steps_, const Vec3& position_, float yRot_, float xRot_)
        : steps(steps_), position(position_), yRot(yRot_), xRot(xRot_) {}

    // decrease() — :128-130.
    void decrease() { this->steps--; }

    // addDelta(Vec3) — :132-134.
    void addDelta(const Vec3& delta) { this->position = this->position.add(delta); }

    // addRotation(float yRot, float xRot) — :136-139.
    void addRotation(float yRot_, float xRot_) {
        this->yRot += yRot_;
        this->xRot += xRot_;
    }
};

// DEFAULT_INTERPOLATION_STEPS — InterpolationHandler.java:11.
inline constexpr int DEFAULT_INTERPOLATION_STEPS = 3;

// Result of one interpolate() tick: the new entity pose plus the updated
// interpolation bookkeeping. previousTickRot is stored as Vec2(xRot, yRot) exactly
// as Java's `new Vec2(entity.getXRot(), entity.getYRot())` (x=xRot, y=yRot).
struct InterpolateStepResult {
    Vec3 newPosition;        // -> entity.setPos
    float newYRot;           // -> entity.setRot (yaw)
    float newXRot;           // -> entity.setRot (pitch)
    InterpolationData data;  // after addDelta/addRotation/decrease
    Vec3 previousTickPosition;  // newPosition (used next tick)
    Vec2 previousTickRot;       // Vec2(entityXRot, entityYRot) read AFTER setRot
};

// Faithful replay of InterpolationHandler.interpolate() (:77-107) for ONE tick,
// decoupled from the live Entity/Level.
//
// Inputs reproduce exactly what interpolate() observes at tick start:
//   data                    — current InterpolationData (must have steps > 0; the
//                             caller's hasActiveInterpolation() guard)
//   entityPos               — entity.position() at the start of this tick
//   entityYRot, entityXRot  — entity.getYRot()/getXRot() at the start of this tick
//   hasPrevPos / prevPos    — previousTickPosition (null-or-value)
//   hasPrevRot / prevRot    — previousTickRot (null-or-value), prevRot = Vec2(xRot,yRot)
//   noCollision             — the result of
//                             entity.level().noCollision(entity,
//                                 entity.makeBoundingBox(data.position + (entityPos - prevPos)))
//                             evaluated by the CALLER (it needs the live level). Only
//                             consulted when hasPrevPos is true.
//
// NOTE: the Java method sets the entity's position BEFORE reading getXRot()/getYRot()
// for previousTickRot, but setPos does not change rotation, so the post-step
// previousTickRot is Vec2(entityXRot, entityYRot) using THIS tick's input rotations
// (setRot writes newYRot/newXRot to the entity but previousTickRot is built from the
// entity's rotation AFTER setRot — i.e. newXRot/newYRot). We mirror that precisely
// below: previousTickRot = Vec2(newXRot, newYRot).
inline InterpolateStepResult interpolateStep(
    InterpolationData data,
    const Vec3& entityPos, float entityYRot, float entityXRot,
    bool hasPrevPos, const Vec3& prevPos,
    bool hasPrevRot, const Vec2& prevRot,
    bool noCollision) {
    // double alpha = 1.0 / this.interpolationData.steps;  — :81
    double alpha = 1.0 / data.steps;

    // previousTickPosition delta, gated by noCollision — :82-87
    if (hasPrevPos) {
        Vec3 deltaSinceLastInterpolation = entityPos.subtract(prevPos);
        if (noCollision) {
            data.addDelta(deltaSinceLastInterpolation);
        }
    }

    // previousTickRot delta — :89-93. prevRot.y = yRot, prevRot.x = xRot.
    if (hasPrevRot) {
        float deltaYRotSinceLastInterpolation = entityYRot - prevRot.y;
        float deltaXRotSinceLastInterpolation = entityXRot - prevRot.x;
        data.addRotation(deltaYRotSinceLastInterpolation, deltaXRotSinceLastInterpolation);
    }

    // lerp the pose toward the (possibly drifted) target — :95-100
    double x = mth::lerp(alpha, entityPos.x, data.position.x);
    double y = mth::lerp(alpha, entityPos.y, data.position.y);
    double z = mth::lerp(alpha, entityPos.z, data.position.z);
    Vec3 newPosition(x, y, z);
    float newYRot = static_cast<float>(mth::rotLerp(alpha, (double)entityYRot, (double)data.yRot));
    float newXRot = static_cast<float>(mth::lerp(alpha, (double)entityXRot, (double)data.xRot));

    // entity.setPos(newPosition); entity.setRot(newYRot, newXRot); — :101-102
    // decrease(); previousTickPosition = newPosition; previousTickRot = Vec2(xRot,yRot) — :103-105
    data.decrease();

    InterpolateStepResult r{
        newPosition,
        newYRot,
        newXRot,
        data,
        newPosition,
        // new Vec2(entity.getXRot(), entity.getYRot()) read AFTER setRot(newYRot,newXRot)
        Vec2(newXRot, newYRot)};
    return r;
}

} // namespace mc
