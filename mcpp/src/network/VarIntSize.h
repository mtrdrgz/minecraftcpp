#pragma once
#include <cstdint>
#include <vector>

// 1:1 port of net.minecraft.network.VarInt (26.1.2).
//
// The existing mc::net::PacketBuffer already ports VarInt.write (writeVarInt);
// this header adds the two pieces PacketBuffer lacks:
//   * getByteSize(int)  — VarInt.java:11-19
//   * the constants     — VarInt.java:6-9
// plus a free-function encode that mirrors VarInt.write byte-for-byte so the
// parity gate can verify the encode path independently of PacketBuffer.
//
// Nothing here is invented: every constant/loop bound/operator is transcribed
// verbatim from VarInt.java.
namespace mc::net::varint {

// VarInt.java:6-9
constexpr int MAX_VARINT_SIZE      = 5;
constexpr int DATA_BITS_MASK       = 127;
constexpr int CONTINUATION_BIT_MASK = 128;
constexpr int DATA_BITS_PER_BYTE   = 7;

// VarInt.java:11-19
//   public static int getByteSize(final int value) {
//      for (int i = 1; i < 5; i++) {
//         if ((value & -1 << i * 7) == 0) {
//            return i;
//         }
//      }
//      return 5;
//   }
// Java: `-1 << (i*7)` is an int shift; the shift amount is masked to 5 bits by
// the JVM (i*7 is at most 28 here so no masking happens), and -1 is 0xFFFFFFFF.
// We reproduce this on uint32_t and compare against zero, matching Java's
// two's-complement int `&`.
inline int getByteSize(int32_t value) {
    for (int i = 1; i < 5; i++) {
        if (((uint32_t)value & ((uint32_t)(-1) << (i * 7))) == 0u) {
            return i;
        }
    }
    return MAX_VARINT_SIZE;
}

// VarInt.java:41-49
//   public static ByteBuf write(final ByteBuf output, int value) {
//      while ((value & -128) != 0) {
//         output.writeByte(value & 127 | 128);
//         value >>>= 7;
//      }
//      output.writeByte(value);
//      return output;
//   }
// `>>>= 7` is Java's logical right shift (unsigned), so we operate on uint32_t.
inline std::vector<uint8_t> write(int32_t value) {
    std::vector<uint8_t> out;
    uint32_t v = (uint32_t)value;
    while ((v & 0xFFFFFF80u) != 0u) {            // value & -128
        out.push_back((uint8_t)((v & 127u) | 128u));
        v >>= 7;                                  // value >>>= 7
    }
    out.push_back((uint8_t)v);                     // output.writeByte(value)
    return out;
}

} // namespace mc::net::varint
