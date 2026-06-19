#pragma once

// 1:1 port of net.minecraft.network.LpVec3 (26.1.2) — "low-precision Vec3"
// network packing. A Vec3 (entity velocity / movement delta) is compressed into
// a variable-length byte sequence: 1 byte if effectively zero, otherwise 6 bytes
// (a 48-bit buffer = lowest byte | middle byte | highest int) plus an optional
// VarInt continuation carrying the high bits of the chessboard scale.
//
// Layout of the 48-bit buffer (LSB first):
//   bits  0..1  : low 2 bits of `scale`            (SCALE_BITS=2, mask 3)
//   bit   2     : CONTINUATION_FLAG (more scale bits follow as a VarInt)
//   bits  3..17 : packed X        (15 bits, X_OFFSET=3)
//   bits 18..32 : packed Y        (15 bits, Y_OFFSET=18)
//   bits 33..47 : packed Z        (15 bits, Z_OFFSET=33)
//
// Each component is quantized to [0, 32766] via pack() and reconstructed to
// [-1, 1] via unpack(), then multiplied by the integer chessboard `scale`.
//
// This is pure int/double arithmetic — no registries, world, or components — so
// it is fully portable. Java semantics replicated bit-exact:
//   * Math.round(double)->long  : the JDK bit-twiddle (jdkRoundD below).
//   * Math.clamp(double,..)     : the JDK !(value>=min) form (sanitize).
//   * Mth.ceilLong(double)      : (long)Math.ceil(v)  -> f2l saturation.
//   * narrowing casts (byte)/(int) wrap two's-complement (handled by uint masks).
//   * VarInt write/read         : 7-bit groups, MSB continuation (LSB-first).
//
// Certified by lp_vec3_parity against real net.minecraft.network.LpVec3.

#include "../world/phys/Vec3.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mc::net {

// A minimal growable byte buffer + read cursor standing in for io.netty.ByteBuf,
// big-endian for multi-byte writes/reads exactly like ByteBuf.writeInt/readInt /
// readUnsignedInt. Only the operations LpVec3 needs are provided.
struct LpByteBuf {
    std::vector<uint8_t> data;
    size_t pos = 0;

    // ── writes (big-endian) ──────────────────────────────────────────────────
    void writeByte(int v) { data.push_back(static_cast<uint8_t>(v)); }           // ByteBuf.writeByte((byte)..)
    void writeInt(int32_t v) {                                                   // ByteBuf.writeInt (BE)
        uint32_t u = static_cast<uint32_t>(v);
        data.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(u & 0xFF));
    }

    // ── reads (big-endian, unsigned where Java reads unsigned) ───────────────
    int readUnsignedByte() { return static_cast<int>(data[pos++]); }             // ByteBuf.readUnsignedByte -> short(0..255)
    int64_t readUnsignedInt() {                                                  // ByteBuf.readUnsignedInt -> long(0..2^32-1)
        uint32_t u = (static_cast<uint32_t>(data[pos]) << 24)
                   | (static_cast<uint32_t>(data[pos + 1]) << 16)
                   | (static_cast<uint32_t>(data[pos + 2]) << 8)
                   | (static_cast<uint32_t>(data[pos + 3]));
        pos += 4;
        return static_cast<int64_t>(static_cast<uint64_t>(u));
    }
    int8_t readSignedByte() { return static_cast<int8_t>(data[pos++]); }         // ByteBuf.readByte -> byte
};

// java.lang.Math.round(double) — the exact JDK bit-twiddle (Math.round, not
// Mth.round). For NaN returns 0; saturates to long for huge magnitudes. Here the
// argument is always finite and in [0, 32766], but we replicate verbatim.
inline int64_t jdkRoundD(double a) {
    constexpr int     SIGNIFICAND_WIDTH = 53;
    constexpr int     EXP_BIAS          = 1023;
    constexpr int64_t EXP_BIT_MASK      = 0x7FF0000000000000LL;
    constexpr int64_t SIGNIF_BIT_MASK   = 0x000FFFFFFFFFFFFFLL;

    int64_t longBits = static_cast<int64_t>(std::bit_cast<uint64_t>(a));
    int64_t biasedExp = (longBits & EXP_BIT_MASK)
                        >> (SIGNIFICAND_WIDTH - 1);
    int64_t shift = (static_cast<int64_t>(SIGNIFICAND_WIDTH) - 2 + EXP_BIAS) - biasedExp;
    if ((shift & -64LL) == 0) { // shift >= 0 && shift < 64
        int64_t r = ((longBits & SIGNIF_BIT_MASK) | (SIGNIF_BIT_MASK + 1));
        if (longBits < 0) {
            r = -r;
        }
        // ((r >> shift) + 1) >> 1  — arithmetic shifts on signed long.
        return ((r >> shift) + 1) >> 1;
    } else {
        // a is either too large for a long or so small it rounds to 0.
        return static_cast<int64_t>(a); // C++ f2l UB only outside range; finite & in-range here
    }
}

// Mth.ceilLong(double) = (long)Math.ceil(v). Java f2l saturates per JLS; finite
// in-range values here, so std::ceil + static_cast suffices (matches Mth.h port).
inline int64_t ceilLong(double v) { return static_cast<int64_t>(std::ceil(v)); }

class LpVec3 {
public:
    static constexpr int    DATA_BITS         = 15;
    static constexpr int    DATA_BITS_MASK    = 32767;
    static constexpr double MAX_QUANTIZED_VALUE = 32766.0;
    static constexpr int    SCALE_BITS        = 2;
    static constexpr int    SCALE_BITS_MASK   = 3;
    static constexpr int    CONTINUATION_FLAG = 4;
    static constexpr int    X_OFFSET          = 3;
    static constexpr int    Y_OFFSET          = 18;
    static constexpr int    Z_OFFSET          = 33;
    static constexpr double ABS_MAX_VALUE     = 1.7179869183E10;
    static constexpr double ABS_MIN_VALUE     = 3.051944088384301E-5;

    static bool hasContinuationBit(int in) {
        return (in & 4) == 4;
    }

    static Vec3 read(LpByteBuf& input) {
        int lowest = input.readUnsignedByte();
        if (lowest == 0) {
            return Vec3{0.0, 0.0, 0.0}; // Vec3.ZERO
        }

        int middle = input.readUnsignedByte();
        int64_t highest = input.readUnsignedInt();
        // long buffer = highest << 16 | middle << 8 | lowest;
        // highest is long (0..2^32-1); middle/lowest are int 0..255 promoted to
        // long. All three operands are non-negative, so the OR is unambiguous.
        int64_t buffer = (highest << 16)
                       | static_cast<int64_t>(middle << 8)
                       | static_cast<int64_t>(lowest);
        int64_t scale = lowest & 3;
        if (hasContinuationBit(lowest)) {
            int varint = varIntRead(input);
            scale |= (static_cast<int64_t>(static_cast<uint32_t>(varint))) << 2;
        }

        double s = static_cast<double>(scale);
        return Vec3{
            unpack(buffer >> 3) * s,
            unpack(buffer >> 18) * s,
            unpack(buffer >> 33) * s,
        };
    }

    static void write(LpByteBuf& output, const Vec3& value) {
        double x = sanitize(value.x);
        double y = sanitize(value.y);
        double z = sanitize(value.z);
        double chessboardLength = absMax(x, absMax(y, z));
        if (chessboardLength < ABS_MIN_VALUE) {
            output.writeByte(0);
        } else {
            int64_t scale = ceilLong(chessboardLength);
            bool isPartial = (scale & 3LL) != scale;
            int64_t markers = isPartial ? (scale & 3LL | 4LL) : scale;
            int64_t xn = pack(x / static_cast<double>(scale)) << 3;
            int64_t yn = pack(y / static_cast<double>(scale)) << 18;
            int64_t zn = pack(z / static_cast<double>(scale)) << 33;
            int64_t buffer = markers | xn | yn | zn;
            output.writeByte(static_cast<int8_t>(buffer));           // (byte)buffer
            output.writeByte(static_cast<int8_t>(buffer >> 8));      // (byte)(buffer >> 8)
            output.writeInt(static_cast<int32_t>(buffer >> 16));     // (int)(buffer >> 16)
            if (isPartial) {
                varIntWrite(output, static_cast<int32_t>(scale >> 2));
            }
        }
    }

private:
    // private static double sanitize(double) — NaN->0, else Math.clamp(v, -MAX, +MAX).
    static double sanitize(double value) {
        if (std::isnan(value)) {
            return 0.0;
        }
        // java.lang.Math.clamp(value, -ABS_MAX_VALUE, ABS_MAX_VALUE):
        //   if (!(value >= min)) value = min; if (value > max) value = max;
        double v = value;
        if (!(v >= -ABS_MAX_VALUE)) {
            v = -ABS_MAX_VALUE;
        }
        if (v > ABS_MAX_VALUE) {
            v = ABS_MAX_VALUE;
        }
        return v;
    }

    // private static long pack(double) = Math.round((value*0.5 + 0.5) * 32766.0).
    static int64_t pack(double value) {
        return jdkRoundD((value * 0.5 + 0.5) * MAX_QUANTIZED_VALUE);
    }

    // private static double unpack(long) =
    //   Math.min(value & 32767, 32766.0) * 2.0 / 32766.0 - 1.0.
    static double unpack(int64_t value) {
        double masked = static_cast<double>(value & 32767LL);
        return std::min(masked, MAX_QUANTIZED_VALUE) * 2.0 / MAX_QUANTIZED_VALUE - 1.0;
    }

    // Mth.absMax(double,double) = Math.max(Math.abs(a), Math.abs(b)).
    static double absMax(double a, double b) {
        return std::max(std::fabs(a), std::fabs(b));
    }

    // ── net.minecraft.network.VarInt (the 32-bit LEB128-style codec) ─────────
    static int varIntRead(LpByteBuf& input) {
        int32_t out = 0;
        int bytes = 0;
        int8_t in;
        do {
            in = input.readSignedByte();
            out |= (static_cast<int32_t>(in & 127)) << (bytes++ * 7);
            // VarInt.read throws if bytes>5 — physical scales never get there.
        } while ((in & 128) == 128);
        return out;
    }
    static void varIntWrite(LpByteBuf& output, int32_t value) {
        while ((value & -128) != 0) {
            output.writeByte((value & 127) | 128);
            value = static_cast<int32_t>(static_cast<uint32_t>(value) >> 7); // >>> 7
        }
        output.writeByte(value);
    }
};

} // namespace mc::net
