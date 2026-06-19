#pragma once

// 1:1 port of the PURE, GL-free first-person hand/item PoseStack transforms from
// net.minecraft.client.renderer.ItemInHandRenderer (Minecraft 26.1.2,
// 26.1.2/src/net/minecraft/client/renderer/ItemInHandRenderer.java).
//
// ItemInHandRenderer as a whole is a renderer (it owns a Minecraft, dispatchers,
// model resolvers and submits geometry), but THREE of its private helpers are
// entirely deterministic CPU math that touch no instance state, no GL/GPU, no
// world: they take only primitives + a PoseStack and either return a float or
// apply a fixed chain of PoseStack translate/scale/mulPose(Axis.*.rotationDegrees)
// operations. Those three are ported here, verified bit-exact against the real
// class (driven on an Unsafe-allocated instance, since they never read `this`):
//
//   * float calculateMapTilt(float xRot)                                (line 145)
//   * void  applyItemArmTransform(PoseStack, HumanoidArm, float)        (line 342)
//   * void  applyItemArmAttackTransform(PoseStack, HumanoidArm, float)  (line 332)
//
// Every value/constant/order comes straight from the decompiled Java; nothing is
// invented, simplified or tuned.
//
// 1:1 traps reproduced here:
//   - Mth.sin / Mth.cos are the TABLE-based net.minecraft.util.Mth.{sin,cos}(double)
//     (SIN[(long)(x*10430.378350470453) & 65535]), NOT std::sin/cos. The float
//     argument widens to double before the multiply — mc::levelgen::mth::sin/cos
//     take a double, so passing a float reproduces that widening exactly.
//   - Mth.sqrt(float) = (float)Math.sqrt((double)x) — mc::levelgen::mth::sqrt.
//   - Mth.clamp(float,float,float) = value<min?min:Math.min(value,max)
//     — mc::levelgen::mth::clamp (the float overload).
//   - Axis.{X,Y,Z}P.rotationDegrees(angle) builds new Quaternionf().rotation{X,Y,Z}
//     (angle * (float)(Math.PI/180.0)) using org.joml's libm-backed jsin/cosFromSin
//     (NOT the Mth table) — that distinction is already certified in MathAxis.h.
//   - (float)Math.PI == 3.1415927f (0x40490fdb); the casts `(float)`/`int invert`
//     and the literal float constants (0.56F, -0.52F, -0.6F, -0.72F, 45.0F, -20.0F,
//     -80.0F, -45.0F, etc.) are transcribed verbatim.
//   - HumanoidArm.RIGHT yields invert = +1, LEFT yields -1 (Java:
//     `arm == HumanoidArm.RIGHT ? 1 : -1`).

#include "render/PoseStack.h"
#include "render/model/MathAxis.h"
#include "world/level/levelgen/Mth.h"

namespace mc::client::renderer {

namespace mth = mc::levelgen::mth;
namespace axis = mc::render::model::math_axis;

// net.minecraft.world.entity.HumanoidArm (ordinals: LEFT=0, RIGHT=1).
enum class HumanoidArm : int { LEFT = 0, RIGHT = 1 };

// (float)Math.PI — the IEEE-754 float nearest to pi (0x40490fdb).
inline constexpr float MTH_PI = 3.1415927f;
// (float)(Math.PI * 2) computed in Java as a double then narrowed to float
// (0x40c90fdb). Used by renderPlayerArm/renderOneHandedMap; not needed here but
// kept consistent with the literal forms in the source.

// ItemInHandRenderer.calculateMapTilt(final float xRot) — line 145.
//   float tilt = 1.0F - xRot / 45.0F + 0.1F;
//   tilt = Mth.clamp(tilt, 0.0F, 1.0F);
//   return -Mth.cos(tilt * (float) Math.PI) * 0.5F + 0.5F;
inline float calculateMapTilt(float xRot) {
    float tilt = 1.0f - xRot / 45.0f + 0.1f;
    tilt = mth::clamp(tilt, 0.0f, 1.0f);
    return -mth::cos(tilt * MTH_PI) * 0.5f + 0.5f;
}

// ItemInHandRenderer.applyItemArmTransform(PoseStack, HumanoidArm, float) — line 342.
//   int invert = arm == HumanoidArm.RIGHT ? 1 : -1;
//   poseStack.translate(invert * 0.56F, -0.52F + inverseArmHeight * -0.6F, -0.72F);
inline void applyItemArmTransform(mc::render::PoseStack& poseStack, HumanoidArm arm,
                                  float inverseArmHeight) {
    int invert = arm == HumanoidArm::RIGHT ? 1 : -1;
    poseStack.translate(invert * 0.56f, -0.52f + inverseArmHeight * -0.6f, -0.72f);
}

// ItemInHandRenderer.applyItemArmAttackTransform(PoseStack, HumanoidArm, float) — line 332.
//   int invert = arm == HumanoidArm.RIGHT ? 1 : -1;
//   float ySwingRotation = Mth.sin(attackValue * attackValue * (float) Math.PI);
//   poseStack.mulPose(Axis.YP.rotationDegrees(invert * (45.0F + ySwingRotation * -20.0F)));
//   float xzSwingRotation = Mth.sin(Mth.sqrt(attackValue) * (float) Math.PI);
//   poseStack.mulPose(Axis.ZP.rotationDegrees(invert * xzSwingRotation * -20.0F));
//   poseStack.mulPose(Axis.XP.rotationDegrees(xzSwingRotation * -80.0F));
//   poseStack.mulPose(Axis.YP.rotationDegrees(invert * -45.0F));
inline void applyItemArmAttackTransform(mc::render::PoseStack& poseStack, HumanoidArm arm,
                                        float attackValue) {
    int invert = arm == HumanoidArm::RIGHT ? 1 : -1;
    float ySwingRotation = mth::sin(attackValue * attackValue * MTH_PI);
    poseStack.mulPose(axis::YP_rotationDegrees(invert * (45.0f + ySwingRotation * -20.0f)));
    float xzSwingRotation = mth::sin(mth::sqrt(attackValue) * MTH_PI);
    poseStack.mulPose(axis::ZP_rotationDegrees(invert * xzSwingRotation * -20.0f));
    poseStack.mulPose(axis::XP_rotationDegrees(xzSwingRotation * -80.0f));
    poseStack.mulPose(axis::YP_rotationDegrees(invert * -45.0f));
}

}  // namespace mc::client::renderer
