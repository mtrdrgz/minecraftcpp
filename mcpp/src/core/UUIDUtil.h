// UUIDUtil.h — 1:1 C++ port of net.minecraft.core.UUIDUtil (MC 26.1.2).
//
// Ported methods (verbatim from 26.1.2/src/net/minecraft/core/UUIDUtil.java):
//   * uuidFromIntArray(int[4])           -> {msb, lsb}
//   * uuidToIntArray(msb, lsb)           -> int[4]   (delegates to leastMostToIntArray)
//   * leastMostToIntArray(msb, lsb)      -> int[4]
//   * createOfflinePlayerUUID(name)      -> UUID.nameUUIDFromBytes("OfflinePlayer:"+name)
//
// createOfflinePlayerUUID relies on java.util.UUID.nameUUIDFromBytes, a type-3
// (MD5 name-based) UUID. The JDK algorithm is reproduced exactly:
//   md = MD5(name); md[6] = (md[6] & 0x0f) | 0x30;  // version 3
//                   md[8] = (md[8] & 0x3f) | 0x80;  // IETF variant
//   msb = big-endian bytes[0..7]; lsb = big-endian bytes[8..15].
//
// The MD5 here is genuine RFC 1321 (replicated verbatim from the certified
// implementation in world/level/levelgen/RandomSource.cpp md5Bytes(); standard
// 16-byte output, each A/B/C/D word little-endian). This is a NEW header per the
// new-files-only rule; the engine's MD5 lives in a .cpp (not a shared header), so
// it is replicated here rather than #included.
//
// Java semantics preserved: longs are signed 64-bit two's-complement; the int[4]
// entries are signed 32-bit. (int)(long) in Java truncates to the low 32 bits,
// which is exactly static_cast<int32_t>(uint64).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mc::uuidutil {

// A 128-bit UUID as Java stores it: signed most/least significant 64-bit halves.
struct Uuid {
    int64_t mostSigBits;
    int64_t leastSigBits;

    bool operator==(const Uuid& o) const {
        return mostSigBits == o.mostSigBits && leastSigBits == o.leastSigBits;
    }
};

// --- UUIDUtil.uuidFromIntArray ---------------------------------------------
// Java:
//   new UUID((long)intArray[0] << 32 | intArray[1] & 4294967295L,
//            (long)intArray[2] << 32 | intArray[3] & 4294967295L)
inline Uuid uuidFromIntArray(const std::array<int32_t, 4>& a) {
    const uint64_t msb = (static_cast<uint64_t>(static_cast<int64_t>(a[0])) << 32)
                       | (static_cast<uint64_t>(static_cast<uint32_t>(a[1])));
    const uint64_t lsb = (static_cast<uint64_t>(static_cast<int64_t>(a[2])) << 32)
                       | (static_cast<uint64_t>(static_cast<uint32_t>(a[3])));
    return Uuid{ static_cast<int64_t>(msb), static_cast<int64_t>(lsb) };
}

// --- UUIDUtil.leastMostToIntArray ------------------------------------------
// Java:
//   new int[]{(int)(mostSignificantBits >> 32), (int)mostSignificantBits,
//             (int)(leastSignificantBits >> 32), (int)leastSignificantBits}
inline std::array<int32_t, 4> leastMostToIntArray(int64_t mostSignificantBits,
                                                  int64_t leastSignificantBits) {
    return {
        static_cast<int32_t>(static_cast<uint64_t>(mostSignificantBits) >> 32),
        static_cast<int32_t>(static_cast<uint64_t>(mostSignificantBits)),
        static_cast<int32_t>(static_cast<uint64_t>(leastSignificantBits) >> 32),
        static_cast<int32_t>(static_cast<uint64_t>(leastSignificantBits)),
    };
}

// --- UUIDUtil.uuidToIntArray -----------------------------------------------
inline std::array<int32_t, 4> uuidToIntArray(const Uuid& uuid) {
    return leastMostToIntArray(uuid.mostSigBits, uuid.leastSigBits);
}

// Genuine RFC 1321 MD5 (replicated verbatim from RandomSource.cpp md5Bytes()).
// Returns the standard 16-byte digest: each A/B/C/D word little-endian.
inline std::array<uint8_t, 16> md5Bytes(const std::string& input) {
    auto rotl = [](uint32_t x, uint32_t c) -> uint32_t { return (x << c) | (x >> (32 - c)); };
    static const uint32_t S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
    static const uint32_t K[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu, 0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu, 0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u, 0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u };

    std::vector<uint8_t> msg(input.begin(), input.end());
    const uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8u;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 0; i < 8; ++i) msg.push_back(static_cast<uint8_t>((bitLen >> (8 * i)) & 0xFF)); // length, little-endian

    uint32_t a0 = 0x67452301u, b0 = 0xefcdab89u, c0 = 0x98badcfeu, d0 = 0x10325476u;
    for (std::size_t off = 0; off < msg.size(); off += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; ++i)
            M[i] = static_cast<uint32_t>(msg[off + i * 4])
                 | (static_cast<uint32_t>(msg[off + i * 4 + 1]) << 8)
                 | (static_cast<uint32_t>(msg[off + i * 4 + 2]) << 16)
                 | (static_cast<uint32_t>(msg[off + i * 4 + 3]) << 24);
        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; ++i) {
            uint32_t F;
            int g;
            if (i < 16)      { F = (B & C) | (~B & D);        g = i; }
            else if (i < 32) { F = (D & B) | (~D & C);        g = (5 * i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D;                 g = (3 * i + 5) % 16; }
            else             { F = C ^ (B | ~D);              g = (7 * i) % 16; }
            F = F + A + K[i] + M[g];
            A = D; D = C; C = B;
            B = B + rotl(F, S[i]);
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    std::array<uint8_t, 16> digest{};
    auto put = [&](int base, uint32_t v) { for (int i = 0; i < 4; ++i) digest[base + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF); };
    put(0, a0); put(4, b0); put(8, c0); put(12, d0);
    return digest;
}

// --- java.util.UUID.nameUUIDFromBytes(byte[]) -------------------------------
// JDK algorithm: MD5(name), set version(3)/variant(IETF) bits, read big-endian.
inline Uuid nameUuidFromBytes(const std::string& name) {
    std::array<uint8_t, 16> md = md5Bytes(name);
    md[6] = static_cast<uint8_t>((md[6] & 0x0f) | 0x30); // clear version, set to v3
    md[8] = static_cast<uint8_t>((md[8] & 0x3f) | 0x80); // clear variant, set IETF
    uint64_t msb = 0, lsb = 0;
    for (int i = 0; i < 8; ++i)  msb = (msb << 8) | static_cast<uint64_t>(md[i]);
    for (int i = 8; i < 16; ++i) lsb = (lsb << 8) | static_cast<uint64_t>(md[i]);
    return Uuid{ static_cast<int64_t>(msb), static_cast<int64_t>(lsb) };
}

// --- UUIDUtil.createOfflinePlayerUUID --------------------------------------
// Java: UUID.nameUUIDFromBytes(("OfflinePlayer:" + playerName).getBytes(UTF_8))
inline Uuid createOfflinePlayerUUID(const std::string& playerName) {
    return nameUuidFromBytes(std::string("OfflinePlayer:") + playerName);
}

} // namespace mc::uuidutil
