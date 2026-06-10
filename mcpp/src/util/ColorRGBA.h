#pragma once

// 1:1 port of net.minecraft.util.ColorRGBA (26.1.2).
//
//   public record ColorRGBA(int rgba) {
//      public String toString() { return HexFormat.of().toHexDigits(this.rgba, 8); }
//   }
//
// A thin typed wrapper around a packed 32-bit ARGB/RGBA int. The CODEC field
// (ExtraCodecs.STRING_ARGB_COLOR.xmap) is serialization-only and depends on
// DataFixerUpper / ExtraCodecs, so it is NOT ported here (listed unported).
//
// The packed int's channel pack/unpack accessors are the certified
// net.minecraft.util.ARGB primitives (mc::argb::), reused read-only — ColorRGBA
// is just the value-class holder over that int. Certified by color_rgba_parity.

#include "ARGB.h"

#include <cstdint>
#include <string>

namespace mc::util {

// java.util.HexFormat.of().toHexDigits(int value, int digits) for digits==8:
// the rightmost 8 hex digits (the full unsigned 32-bit value), lowercase,
// zero-padded. HexFormat.of() uses the lowercase digit set "0123456789abcdef".
inline std::string toHexDigits8(int value) {
    static const char DIGITS[] = "0123456789abcdef";
    auto v = static_cast<uint32_t>(value);
    std::string out(8, '0');
    for (int i = 7; i >= 0; --i) {
        out[i] = DIGITS[v & 0xF];
        v >>= 4;
    }
    return out;
}

// public record ColorRGBA(int rgba)
struct ColorRGBA {
    int rgba;

    explicit constexpr ColorRGBA(int rgba_) : rgba(rgba_) {}

    // record accessor
    constexpr int rgbaValue() const { return rgba; }

    // public String toString() — HexFormat.of().toHexDigits(this.rgba, 8)
    std::string toString() const { return toHexDigits8(rgba); }

    // record component equality (int identity)
    constexpr bool operator==(const ColorRGBA& o) const { return rgba == o.rgba; }
    constexpr bool operator!=(const ColorRGBA& o) const { return rgba != o.rgba; }

    // Packed-channel accessors over the held int, delegating to the certified
    // net.minecraft.util.ARGB primitives (the canonical RGBA int pack/unpack).
    int alpha() const { return mc::argb::alpha(rgba); }
    int red()   const { return mc::argb::red(rgba); }
    int green() const { return mc::argb::green(rgba); }
    int blue()  const { return mc::argb::blue(rgba); }
};

} // namespace mc::util
