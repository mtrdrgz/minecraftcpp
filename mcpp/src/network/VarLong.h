#pragma once
// 1:1 port of net.minecraft.network.VarLong (Minecraft 26.1.2).
//
// VarLong.java verbatim:
//   private static final int MAX_VARLONG_SIZE   = 10;
//   private static final int DATA_BITS_MASK     = 127;
//   private static final int CONTINUATION_BIT_MASK = 128;
//   private static final int DATA_BITS_PER_BYTE = 7;
//
//   public static int getByteSize(final long value) {
//      for (int i = 1; i < 10; i++) {
//         if ((value & -1L << i * 7) == 0L) {
//            return i;
//         }
//      }
//      return 10;
//   }
//
//   public static ByteBuf write(final ByteBuf output, long value) {
//      while ((value & -128L) != 0L) {
//         output.writeByte((int)(value & 127L) | 128);
//         value >>>= 7;
//      }
//      output.writeByte((int)value);
//      return output;
//   }
//
// This header is self-contained (no engine deps) so the parity gate is standalone.
// Note: the engine's runtime encoder lives in network/PacketBuffer.cpp
// (PacketBuffer::writeVarLong); this header re-states the same algorithm verbatim
// for the bit-exact gate. They are kept structurally identical on purpose.

#include <cstdint>
#include <cstddef>
#include <vector>

namespace mc::net::varlong {

inline constexpr int MAX_VARLONG_SIZE = 10;
inline constexpr int DATA_BITS_MASK = 127;
inline constexpr int CONTINUATION_BIT_MASK = 128;
inline constexpr int DATA_BITS_PER_BYTE = 7;

// VarLong.getByteSize — note Java shifts a signed long by (i*7) which for i in
// [1,9] is [7,63], all in-range; `-1L << (i*7)` produces a mask of the high
// (64 - i*7) bits. If those are all zero the value fits in i bytes.
inline int getByteSize(const int64_t value) {
    for (int i = 1; i < 10; i++) {
        if ((value & (int64_t)(-1LL << (i * 7))) == 0LL) {
            return i;
        }
    }
    return 10;
}

// VarLong.write — appends the LEB128-style bytes (low 7 bits first, continuation
// bit set while more groups remain). `>>>= 7` is Java's unsigned right shift, so
// the C++ side operates on the unsigned bit pattern.
inline void write(std::vector<uint8_t>& out, int64_t value) {
    uint64_t v = static_cast<uint64_t>(value);
    while ((value & -128LL) != 0LL) {
        out.push_back(static_cast<uint8_t>((int)(value & 127LL) | 128));
        v >>= 7;                      // value >>>= 7 (unsigned)
        value = static_cast<int64_t>(v);
    }
    out.push_back(static_cast<uint8_t>((int)value));
}

} // namespace mc::net::varlong
