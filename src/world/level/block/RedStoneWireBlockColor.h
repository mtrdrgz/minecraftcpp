#pragma once

// 1:1 port of net.minecraft.world.level.block.RedStoneWireBlock's static power->color
// table (26.1.2). This is the COLORS[16] table built in the class initializer plus the
// public accessor RedStoneWireBlock.getColorForPower(int):
//
//   COLORS = new int[16]; for (i = 0..15) {
//      float power = i / 15.0F;
//      float red   = power * 0.6F + (power > 0.0F ? 0.4F : 0.3F);
//      float green = Mth.clamp(power * power * 0.7F - 0.5F, 0.0F, 1.0F);
//      float blue  = Mth.clamp(power * power * 0.6F - 0.7F, 0.0F, 1.0F);
//      COLORS[i] = ARGB.colorFromFloat(1.0F, red, green, blue);
//   }
//   public static int getColorForPower(int power) { return COLORS[power]; }
//
// PURE: no world/BlockGetter, no registry, no GL. Float arithmetic in IEEE-754
// single precision, float->int truncation via Mth.floor (which widens to double),
// Mth.clamp(float) semantics (value<min?min:min(value,max)), and ARGB byte packing.
//
// Reuses the certified mc::argb / mc::levelgen::mth ports for the leaf ops, so the only
// thing this file adds is the exact float expression chain and the table build/index.
//
// Certified byte-exact by redstone_wire_color_parity.

#include "../../../util/ARGB.h"
#include "../levelgen/Mth.h"

#include <array>

namespace mc::world::level::block {

namespace mth = mc::levelgen::mth;
namespace argb = mc::argb;

// Builds COLORS[16] exactly as RedStoneWireBlock's static initializer does.
inline std::array<int, 16> buildRedstoneWireColors() {
    std::array<int, 16> colors{};
    for (int i = 0; i <= 15; i++) {
        float power = i / 15.0F;
        float red   = power * 0.6F + (power > 0.0F ? 0.4F : 0.3F);
        float green = mth::clamp(power * power * 0.7F - 0.5F, 0.0F, 1.0F);
        float blue  = mth::clamp(power * power * 0.6F - 0.7F, 0.0F, 1.0F);
        colors[i] = argb::colorFromFloat(1.0F, red, green, blue);
    }
    return colors;
}

// RedStoneWireBlock.getColorForPower(int) — indexes the precomputed table.
// (Mirrors Java: COLORS is built once in the class initializer; the accessor is a
//  bare array read. We build lazily on first call to match a single dump.)
inline int getColorForPower(int power) {
    static const std::array<int, 16> COLORS = buildRedstoneWireColors();
    return COLORS[power];
}

}  // namespace mc::world::level::block
