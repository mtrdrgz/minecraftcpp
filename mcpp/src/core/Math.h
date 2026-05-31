#pragma once
#include <cstdint>
#include <functional>

// GLM defines are set globally via CMake compile definitions
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

namespace mc {

// Block-coordinate integer position (ported from BlockPos.java)
struct BlockPos {
    int32_t x, y, z;
    constexpr bool operator==(const BlockPos&) const = default;
};

// Chunk XZ position
struct ChunkPos {
    int32_t x, z;
    constexpr bool operator==(const ChunkPos&) const = default;
    static constexpr ChunkPos fromBlock(int32_t bx, int32_t bz) noexcept {
        return {bx >> 4, bz >> 4};
    }
};

// Section position (16^3 sub-chunk)
struct SectionPos {
    int32_t x, y, z;
    constexpr bool operator==(const SectionPos&) const = default;
};

} // namespace mc

namespace std {
template<> struct hash<mc::BlockPos> {
    size_t operator()(const mc::BlockPos& p) const noexcept {
        size_t h = std::hash<int32_t>{}(p.x);
        h ^= std::hash<int32_t>{}(p.y) + 0x9e3779b9 + (h<<6) + (h>>2);
        h ^= std::hash<int32_t>{}(p.z) + 0x9e3779b9 + (h<<6) + (h>>2);
        return h;
    }
};
template<> struct hash<mc::ChunkPos> {
    size_t operator()(const mc::ChunkPos& p) const noexcept {
        return std::hash<int64_t>{}((int64_t)p.x << 32 | (uint32_t)p.z);
    }
};
} // namespace std
