#pragma once

// 1:1 port of the PURE beam-height oscillator of
// net.minecraft.client.renderer.entity.EndCrystalRenderer (Minecraft 26.1.2,
// 26.1.2/src/net/minecraft/client/renderer/entity/EndCrystalRenderer.java).
//
// EndCrystalRenderer as a whole owns a baked EndCrystalModel and submits geometry
// (PoseStack / SubmitNodeCollector / GL), but one of its members is entirely
// deterministic, GL-free CPU math: the public static helper getY(float), which the
// renderer uses to drive the vertical bob of an end-crystal's tether beam:
//
//   public static float getY(final float timeInTicks) {       (EndCrystalRenderer.java:50)
//      float hh = Mth.sin(timeInTicks * 0.2F) / 2.0F + 0.5F;
//      hh = (hh * hh + hh) * 0.4F;
//      return hh - 1.4F;
//   }
//
// It reads no `this` state and no world — its entire output is a function of the
// single float `timeInTicks` (an entity age in ticks + partialTicks). Ported here
// and verified bit-exact against the real class (end_crystal_beam_parity, ground
// truth tools/EndCrystalBeamParity.java driving the REAL EndCrystalRenderer.getY).
//
// Every value/constant/order comes straight from the decompiled Java; nothing is
// invented, simplified or tuned.
//
// 1:1 traps reproduced here:
//   - Mth.sin is the TABLE-based net.minecraft.util.Mth.sin(double)
//     (SIN[(long)(x*10430.378350470453) & 65535]), NOT std::sin. The argument is the
//     float `timeInTicks * 0.2F` (a float multiply), which then WIDENS to double for
//     the table-index multiply — mc::levelgen::mth::sin takes a double, so passing the
//     float reproduces that widening exactly.
//   - `/ 2.0F`, `+ 0.5F`, `* 0.4F`, `- 1.4F` and the `0.2F` factor are all single-
//     precision float operations on float operands; the intermediate `hh` is a float,
//     so `hh * hh + hh` is float multiply + float add (no double promotion).
//   - 0.2F, 2.0F, 0.5F, 0.4F, 1.4F are transcribed verbatim as float literals.
//
// SKIPPED (GL / model-submit path, no pure math to port): the constructor, submit(...),
// createRenderState(), extractRenderState(...), shouldRender(...). Those touch the
// baked model, PoseStack, SubmitNodeCollector and the beam-mesh emission and carry no
// portable numeric algorithm beyond getY — listed as unported.

#include "world/level/levelgen/Mth.h"

namespace mc::client::renderer::entity {

namespace mth = mc::levelgen::mth;

// Pure body of EndCrystalRenderer.getY(float). (EndCrystalRenderer.java:50-54)
//
//   float hh = Mth.sin(timeInTicks * 0.2F) / 2.0F + 0.5F;
//   hh = (hh * hh + hh) * 0.4F;
//   return hh - 1.4F;
inline float getY(float timeInTicks) {
    float hh = mth::sin(timeInTicks * 0.2F) / 2.0F + 0.5F;
    hh = (hh * hh + hh) * 0.4F;
    return hh - 1.4F;
}

}  // namespace mc::client::renderer::entity
