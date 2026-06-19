#pragma once

// 1:1 port of net.minecraft.util.CommonColors (26.1.2) — the named ARGB int color
// constants shared across GUI/text/highlight rendering. Pure data: each is a packed
// 0xAARRGGBB int, stored here exactly as the Java `public static final int` literals
// (signed two's-complement, so the negative decimals in the source are the same bit
// patterns). Certified byte-for-byte by common_colors_parity.
//
// Source: 26.1.2/src/net/minecraft/util/CommonColors.java

#include <cstdint>

namespace mc::common_colors {

inline constexpr std::int32_t WHITE = -1;
inline constexpr std::int32_t BLACK = -16777216;
inline constexpr std::int32_t GRAY = -8355712;
inline constexpr std::int32_t DARK_GRAY = -12566464;
inline constexpr std::int32_t LIGHT_GRAY = -6250336;
inline constexpr std::int32_t LIGHTER_GRAY = -4539718;
inline constexpr std::int32_t RED = -65536;
inline constexpr std::int32_t SOFT_RED = -2142128;
inline constexpr std::int32_t GREEN = -16711936;
inline constexpr std::int32_t BLUE = -16776961;
inline constexpr std::int32_t YELLOW = -256;
inline constexpr std::int32_t SOFT_YELLOW = -171;
inline constexpr std::int32_t DARK_PURPLE = -11534256;
inline constexpr std::int32_t HIGH_CONTRAST_DIAMOND = -11010079;
inline constexpr std::int32_t COSMOS_PINK = -13108;
inline constexpr std::int32_t TEXT_GRAY = -2039584;

}  // namespace mc::common_colors
