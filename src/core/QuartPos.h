#pragma once
// 1:1 port of net.minecraft.core.QuartPos (Minecraft 26.1.2).
//
// Biome-quart coordinate packing. A "quart" is a 4x4x4 block cell — the
// resolution at which biomes are stored. Every method is pure integer bit
// arithmetic translated VERBATIM from the Java:
//
//   public static int fromBlock(int blockCoord)  { return blockCoord >> 2; }
//   public static int quartLocal(int blockCoord) { return blockCoord & 3; }
//   public static int toBlock(int quart)         { return quart << 2; }
//   public static int fromSection(int section)   { return section << 2; }
//   public static int toSection(int quart)       { return quart >> 2; }
//
// Java int == int32_t. Java `>>` is an arithmetic (sign-propagating) right
// shift; C++ `>>` on a signed int32_t is implementation-defined but is an
// arithmetic shift on every target this repo builds for (two's complement,
// llvm-mingw). `<<` and `&` match Java exactly for int32_t.

#include <cstdint>

namespace mc::quartpos {

// public static final int BITS = 2;
inline constexpr int32_t BITS = 2;
// public static final int SIZE = 4;
inline constexpr int32_t SIZE = 4;
// public static final int MASK = 3;
inline constexpr int32_t MASK = 3;
// private static final int SECTION_TO_QUARTS_BITS = 2;
inline constexpr int32_t SECTION_TO_QUARTS_BITS = 2;

// blockCoord >> 2
inline constexpr int32_t fromBlock(int32_t blockCoord) noexcept {
    return blockCoord >> 2;
}

// blockCoord & 3
inline constexpr int32_t quartLocal(int32_t blockCoord) noexcept {
    return blockCoord & 3;
}

// quart << 2
inline constexpr int32_t toBlock(int32_t quart) noexcept {
    return quart << 2;
}

// section << 2
inline constexpr int32_t fromSection(int32_t section) noexcept {
    return section << 2;
}

// quart >> 2
inline constexpr int32_t toSection(int32_t quart) noexcept {
    return quart >> 2;
}

} // namespace mc::quartpos
