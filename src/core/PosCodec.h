#pragma once

// 1:1 port of the long-packing / coordinate codecs that underpin chunk & region
// serialization, network packets, and every long-keyed position map in Minecraft
// 26.1.2:
//   net.minecraft.core.BlockPos      (asLong/getX/getY/getZ/offset/getFlatIndex)
//   net.minecraft.world.level.ChunkPos (asLong/getX/getZ + long ctor)
//   net.minecraft.core.SectionPos    (asLong/x/y/z, blockToSectionCoord,
//                                     sectionToBlockCoord, sectionRelative*)
//
// Pure bit arithmetic — packing built unsigned for well-defined wrap, the
// field-extraction left-shift-then-arithmetic-right-shift sign-extends exactly as
// Java's `(int)(node << a >> b)`. Certified bit-exact by pos_codec_parity.

#include <cstdint>
#include <cmath>

namespace mc::poscodec {

// ── BlockPos (BlockPos.java:49-118) ──────────────────────────────────────────
// PACKED_HORIZONTAL_LENGTH = 1 + log2(smallestEncompassingPowerOfTwo(30000000)) = 26
// PACKED_Y_LENGTH = 64 - 2*26 = 12 ; X_OFFSET = 12+26 = 38 ; Z_OFFSET = 12.
inline constexpr int      BP_HORIZ    = 26;
inline constexpr int      BP_Y_LEN    = 12;
inline constexpr int      BP_X_OFFSET = 38;
inline constexpr int      BP_Z_OFFSET = 12;
inline constexpr int64_t  BP_X_MASK   = (1LL << BP_HORIZ) - 1LL;   // 0x3FFFFFF
inline constexpr int64_t  BP_Y_MASK   = (1LL << BP_Y_LEN) - 1LL;   // 0xFFF
inline constexpr int      BP_MAX_HORIZONTAL = (1 << BP_HORIZ) / 2 - 1; // 33554431

// BlockPos.asLong(int,int,int) — BlockPos.java:111-116.
inline int64_t blockPosAsLong(int x, int y, int z) {
    uint64_t node = 0;
    node |= (static_cast<uint64_t>(static_cast<int64_t>(x) & BP_X_MASK)) << BP_X_OFFSET;
    node |= (static_cast<uint64_t>(static_cast<int64_t>(y) & BP_Y_MASK)) << 0;
    node |= (static_cast<uint64_t>(static_cast<int64_t>(z) & BP_X_MASK)) << BP_Z_OFFSET;
    return static_cast<int64_t>(node);
}
// Helper: Java `(int)(node << leftShift >> rightShift)` (left unsigned, right arithmetic).
inline int signExtractInt(int64_t node, int leftShift, int rightShift) {
    int64_t v = static_cast<int64_t>(static_cast<uint64_t>(node) << leftShift);
    return static_cast<int>(v >> rightShift);
}
// BlockPos.getX/getY/getZ — BlockPos.java:75-85.
inline int blockPosGetX(int64_t node) { return signExtractInt(node, 64 - BP_X_OFFSET - BP_HORIZ, 64 - BP_HORIZ); } // <<0  >>38
inline int blockPosGetY(int64_t node) { return signExtractInt(node, 64 - BP_Y_LEN, 64 - BP_Y_LEN); }              // <<52 >>52
inline int blockPosGetZ(int64_t node) { return signExtractInt(node, 64 - BP_Z_OFFSET - BP_HORIZ, 64 - BP_HORIZ); }// <<26 >>38
// BlockPos.offset(long,int,int,int) — BlockPos.java:71-73.
inline int64_t blockPosOffset(int64_t node, int sx, int sy, int sz) {
    return blockPosAsLong(blockPosGetX(node) + sx, blockPosGetY(node) + sy, blockPosGetZ(node) + sz);
}
// BlockPos.getFlatIndex(long) — BlockPos.java:118-120.
inline int64_t blockPosGetFlatIndex(int64_t node) { return node & -16LL; }

// ── ChunkPos (ChunkPos.java:53,77,84-89) ─────────────────────────────────────
// asLong(x,z) = (x & 0xFFFFFFFFL) | (z & 0xFFFFFFFFL) << 32.
inline int64_t chunkPosAsLong(int x, int z) {
    return (static_cast<int64_t>(x) & 0xFFFFFFFFLL) | ((static_cast<int64_t>(z) & 0xFFFFFFFFLL) << 32);
}
inline int chunkPosGetX(int64_t key) { return static_cast<int>(key); }
inline int chunkPosGetZ(int64_t key) { return static_cast<int>(static_cast<uint64_t>(key) >> 32); }

// ── SectionPos (SectionPos.java:77-220) ──────────────────────────────────────
// asLong(x,y,z): x&0x3FFFFF (22) <<42 | y&0xFFFFF (20) | z&0x3FFFFF <<20.
inline constexpr int64_t SP_XZ_MASK = 0x3FFFFFLL; // 22 bits
inline constexpr int64_t SP_Y_MASK  = 0xFFFFFLL;  // 20 bits
inline int64_t sectionPosAsLong(int x, int y, int z) {
    uint64_t node = 0;
    node |= (static_cast<uint64_t>(static_cast<int64_t>(x) & SP_XZ_MASK)) << 42;
    node |= (static_cast<uint64_t>(static_cast<int64_t>(y) & SP_Y_MASK)) << 0;
    node |= (static_cast<uint64_t>(static_cast<int64_t>(z) & SP_XZ_MASK)) << 20;
    return static_cast<int64_t>(node);
}
inline int sectionPosX(int64_t node) { return signExtractInt(node, 0, 42); }  // <<0  >>42
inline int sectionPosY(int64_t node) { return signExtractInt(node, 44, 44); } // <<44 >>44
inline int sectionPosZ(int64_t node) { return signExtractInt(node, 22, 42); } // <<22 >>42

// blockToSectionCoord / sectionToBlockCoord — SectionPos.java:80-132.
inline int blockToSectionCoord(int blockCoord) { return blockCoord >> 4; }
inline int blockToSectionCoord(double coord) { return static_cast<int>(std::floor(coord)) >> 4; }
inline int sectionToBlockCoord(int sectionCoord) { return sectionCoord << 4; }
inline int sectionToBlockCoord(int sectionCoord, int offset) { return (sectionCoord << 4) + offset; }

// sectionRelative + sectionRelativePos + sectionRelativeX/Y/Z — SectionPos.java:88-106.
inline int sectionRelative(int blockCoord) { return blockCoord & 15; }
inline int16_t sectionRelativePos(int blockX, int blockY, int blockZ) {
    int x = sectionRelative(blockX), y = sectionRelative(blockY), z = sectionRelative(blockZ);
    return static_cast<int16_t>((x << 8) | (z << 4) | (y << 0));
}
// Java `relative >>> N & 15`: short promotes to (sign-extended) int, then logical shift.
inline int sectionRelativeX(int16_t relative) { return (static_cast<uint32_t>(static_cast<int>(relative)) >> 8) & 15; }
inline int sectionRelativeY(int16_t relative) { return (static_cast<uint32_t>(static_cast<int>(relative)) >> 0) & 15; }
inline int sectionRelativeZ(int16_t relative) { return (static_cast<uint32_t>(static_cast<int>(relative)) >> 4) & 15; }

} // namespace mc::poscodec
