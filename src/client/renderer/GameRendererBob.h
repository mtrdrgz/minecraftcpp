#pragma once

// 1:1 port of the PURE, GL-free camera view-bob PoseStack transforms from
// net.minecraft.client.renderer.GameRenderer (Minecraft 26.1.2,
// 26.1.2/src/net/minecraft/client/renderer/GameRenderer.java).
//
// GameRenderer as a whole owns a Minecraft, dispatchers, render targets and submits
// geometry, but TWO of its private helpers are entirely deterministic CPU math:
// they read only plain primitive fields off the CameraEntityRenderState (and one
// double off OptionsRenderState) and apply a fixed chain of PoseStack translate /
// mulPose(Axis.*.rotationDegrees) operations. No GL/GPU, no world, no `this` state
// beyond that single options double. Those two are ported here, verified bit-exact
// against the real class (driven on an Unsafe-allocated GameRenderer, since aside
// from gameRenderState.optionsRenderState.damageTiltStrength they never read `this`):
//
//   * void bobHurt(CameraRenderState, PoseStack)   (GameRenderer.java line 315)
//   * void bobView(CameraRenderState, PoseStack)   (GameRenderer.java line 337)
//
// The CameraRenderState/CameraEntityRenderState/OptionsRenderState fields the two
// methods consult are passed in as explicit parameters (their exact Java types):
//
//   bobHurt reads cameraState.entityRenderState.{isLiving (boolean),
//     isDeadOrDying (boolean), deathTime (float), hurtTime (float),
//     hurtDuration (int), hurtDir (float)} and
//     this.gameRenderState.optionsRenderState.damageTiltStrength (double).
//   bobView reads cameraState.entityRenderState.{isPlayer (boolean),
//     backwardsInterpolatedWalkDistance (float), bob (float)}.
//
// Every value/constant/order comes straight from the decompiled Java; nothing is
// invented, simplified or tuned.
//
// 1:1 traps reproduced here:
//   - Mth.sin / Mth.cos are the TABLE-based net.minecraft.util.Mth.{sin,cos}(double)
//     (SIN[(long)(x*10430.378350470453) & 65535]), NOT std::sin/cos. The float
//     argument widens to double before the table index multiply — the C++
//     mc::levelgen::mth::sin/cos take a double, so passing the float argument
//     reproduces that widening exactly.
//   - Axis.{X,Y,Z}P.rotationDegrees(angle) builds new Quaternionf().rotation{X,Y,Z}
//     (angle * (float)(Math.PI/180.0)) using org.joml's libm-backed jsin/cosFromSin
//     (NOT the Mth table) — that distinction is already certified in MathAxis.h.
//   - (float)Math.PI == 3.1415927f (0x40490fdb). All other literals (0.5F, 3.0F,
//     0.2F, 5.0F, 40.0F, 8000.0F, 200.0F, 20.0F, 14.0, 0.0F) are transcribed verbatim.
//   - bobHurt's tilt amount is `(float)(-hurt * 14.0 * damageTiltStrength)`:
//     hurt (float) promotes to double, * 14.0 (double) * damageTiltStrength (double),
//     then a single (float) narrowing. damageTiltStrength is a DOUBLE field.
//   - bobHurt's `hurt /= hurtDuration` divides a float by an int (hurtDuration),
//     i.e. float / (float)int — int promotes to float, float division.
//   - Math.min(deathTime, 20.0F) and Math.abs(...) act on floats here (float overloads).
//   - bobHurt's hurt<0 early-return short-circuits BEFORE any rotation when hurtTime<0
//     (after the optional death-spin), exactly as the Java does.
//
// The PoseStack glue (translate / mulPose(Quaternionf) -> Pose.rotate updating both
// the pose Matrix4f and the normal Matrix3f) is the certified render/PoseStack.h.

#include "render/PoseStack.h"
#include "render/model/MathAxis.h"
#include "world/level/levelgen/Mth.h"

#include <cmath>

namespace mc::client::renderer {

namespace mth = mc::levelgen::mth;
namespace axis = mc::render::model::math_axis;

// (float)Math.PI — the IEEE-754 float nearest to pi (0x40490fdb).
inline constexpr float GR_PI = 3.1415927f;

// The exact CameraEntityRenderState fields bobHurt/bobView read, in their Java types.
// (Mirrors net.minecraft.client.renderer.state.level.CameraEntityRenderState.)
struct CameraEntityState {
    bool  isLiving = false;
    bool  isPlayer = false;
    bool  isDeadOrDying = false;
    float hurtTime = 0.0f;
    int   hurtDuration = 0;
    float deathTime = 0.0f;
    float hurtDir = 0.0f;
    float backwardsInterpolatedWalkDistance = 0.0f;
    float bob = 0.0f;
};

// GameRenderer.bobHurt(final CameraRenderState cameraState, final PoseStack poseStack) — line 315.
//   if (cameraState.entityRenderState.isLiving) {
//      float hurt = cameraState.entityRenderState.hurtTime;
//      if (cameraState.entityRenderState.isDeadOrDying) {
//         float duration = Math.min(cameraState.entityRenderState.deathTime, 20.0F);
//         poseStack.mulPose(Axis.ZP.rotationDegrees(40.0F - 8000.0F / (duration + 200.0F)));
//      }
//      if (hurt < 0.0F) { return; }
//      hurt /= cameraState.entityRenderState.hurtDuration;
//      hurt = Mth.sin(hurt * hurt * hurt * hurt * (float) Math.PI);
//      float rr = cameraState.entityRenderState.hurtDir;
//      poseStack.mulPose(Axis.YP.rotationDegrees(-rr));
//      float tiltAmount = (float)(-hurt * 14.0 * this.gameRenderState.optionsRenderState.damageTiltStrength);
//      poseStack.mulPose(Axis.ZP.rotationDegrees(tiltAmount));
//      poseStack.mulPose(Axis.YP.rotationDegrees(rr));
//   }
inline void bobHurt(const CameraEntityState& e, double damageTiltStrength,
                    mc::render::PoseStack& poseStack) {
    if (e.isLiving) {
        float hurt = e.hurtTime;
        if (e.isDeadOrDying) {
            float duration = std::fmin(e.deathTime, 20.0f);
            poseStack.mulPose(axis::ZP_rotationDegrees(40.0f - 8000.0f / (duration + 200.0f)));
        }

        if (hurt < 0.0f) {
            return;
        }

        hurt /= static_cast<float>(e.hurtDuration);
        hurt = mth::sin(hurt * hurt * hurt * hurt * GR_PI);
        float rr = e.hurtDir;
        poseStack.mulPose(axis::YP_rotationDegrees(-rr));
        float tiltAmount = static_cast<float>(-static_cast<double>(hurt) * 14.0 * damageTiltStrength);
        poseStack.mulPose(axis::ZP_rotationDegrees(tiltAmount));
        poseStack.mulPose(axis::YP_rotationDegrees(rr));
    }
}

// GameRenderer.bobView(final CameraRenderState cameraState, final PoseStack poseStack) — line 337.
//   if (cameraState.entityRenderState.isPlayer) {
//      float backwardsInterpolatedWalkDistance = cameraState.entityRenderState.backwardsInterpolatedWalkDistance;
//      float bob = cameraState.entityRenderState.bob;
//      poseStack.translate(
//         Mth.sin(backwardsInterpolatedWalkDistance * (float) Math.PI) * bob * 0.5F,
//         -Math.abs(Mth.cos(backwardsInterpolatedWalkDistance * (float) Math.PI) * bob),
//         0.0F);
//      poseStack.mulPose(Axis.ZP.rotationDegrees(Mth.sin(backwardsInterpolatedWalkDistance * (float) Math.PI) * bob * 3.0F));
//      poseStack.mulPose(Axis.XP.rotationDegrees(Math.abs(Mth.cos(backwardsInterpolatedWalkDistance * (float) Math.PI - 0.2F) * bob) * 5.0F));
//   }
inline void bobView(const CameraEntityState& e, mc::render::PoseStack& poseStack) {
    if (e.isPlayer) {
        float backwardsInterpolatedWalkDistance = e.backwardsInterpolatedWalkDistance;
        float bob = e.bob;
        poseStack.translate(
            mth::sin(backwardsInterpolatedWalkDistance * GR_PI) * bob * 0.5f,
            -std::fabs(mth::cos(backwardsInterpolatedWalkDistance * GR_PI) * bob),
            0.0f);
        poseStack.mulPose(axis::ZP_rotationDegrees(mth::sin(backwardsInterpolatedWalkDistance * GR_PI) * bob * 3.0f));
        poseStack.mulPose(axis::XP_rotationDegrees(std::fabs(mth::cos(backwardsInterpolatedWalkDistance * GR_PI - 0.2f) * bob) * 5.0f));
    }
}

}  // namespace mc::client::renderer
