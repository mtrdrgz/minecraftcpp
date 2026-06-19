// 1:1 C++ port of net.minecraft.world.level.LightLayer (Minecraft 26.1.2).
// New file — does not edit any shared header.
//
// Source (26.1.2/src/net/minecraft/world/level/LightLayer.java) — VERBATIM:
//
//   package net.minecraft.world.level;
//   public enum LightLayer {
//      SKY,
//      BLOCK;
//   }
//
// It is a bare enum: no fields, no constructor, no StringRepresentable, no
// accessors. The only portable facts are the two constants, their declaration
// order (ordinals SKY=0, BLOCK=1) and their names. That is the entire data
// surface of the class, all gated by world/level/LightLayerParityTest.cpp.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::world::level {

// Declaration order in LightLayer.java => SKY.ordinal()==0, BLOCK.ordinal()==1.
enum class LightLayer : int32_t {
    SKY = 0,
    BLOCK = 1,
};

// values() ordering == declaration order.
inline constexpr std::array<LightLayer, 2> LIGHT_LAYER_VALUES = {
    LightLayer::SKY,
    LightLayer::BLOCK,
};

inline constexpr int LIGHT_LAYER_COUNT = 2;

// Enum.name() for each constant (the bare identifier — no StringRepresentable).
constexpr std::string_view lightLayerName(LightLayer l) {
    switch (l) {
        case LightLayer::SKY:   return "SKY";
        case LightLayer::BLOCK: return "BLOCK";
    }
    return "";
}

// Enum.ordinal().
constexpr int32_t lightLayerOrdinal(LightLayer l) {
    return static_cast<int32_t>(l);
}

}  // namespace mc::world::level
