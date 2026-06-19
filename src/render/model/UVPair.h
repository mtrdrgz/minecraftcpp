// 1:1 port of net.minecraft.client.model.geom.builders.UVPair (the packed-UV used by
// BakedQuad: packedUV0..3 = UVPair.pack(sprite.getU(u), sprite.getV(v))).
//   pack(u,v)  = (floatToIntBits(u) & 0xFFFFFFFF) << 32 | (floatToIntBits(v) & 0xFFFFFFFF)
//   unpackU(p) = intBitsToFloat((int)(p >> 32))
//   unpackV(p) = intBitsToFloat((int) p)
// (UVPair.java:9-22). Pure bit math, no GL/atlas.
#pragma once

#include <bit>
#include <cstdint>

namespace mc::render::model::uvpair {

inline uint64_t pack(float u, float v) {
    uint64_t high = (uint64_t)std::bit_cast<uint32_t>(u);        // & 0xFFFFFFFF is implicit (uint32->uint64)
    uint64_t low  = (uint64_t)std::bit_cast<uint32_t>(v);
    return (high << 32) | low;
}

inline float unpackU(uint64_t packedUV) {
    return std::bit_cast<float>((uint32_t)(packedUV >> 32));
}

inline float unpackV(uint64_t packedUV) {
    return std::bit_cast<float>((uint32_t)packedUV);
}

}  // namespace mc::render::model::uvpair
