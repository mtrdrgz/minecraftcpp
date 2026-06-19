#pragma once
// Bit-exact 1:1 port of the coordinate helpers on
// net.minecraft.world.level.ChunkPos (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/level/ChunkPos.java
//   getMinBlockX/Z   -> SectionPos.sectionToBlockCoord(this.x)        = x << 4
//   getMaxBlockX/Z   -> getBlockX(15)                                = (x << 4) + 15
//   getMiddleBlockX/Z-> getBlockX(8)                                 = (x << 4) + 8
//   getBlockX/Z(off) -> SectionPos.sectionToBlockCoord(this.x, off)  = (x << 4) + off
//   getRegionX/Z     -> this.x >> 5
//   getRegionLocalX/Z-> this.x & 31
//   getChessboardDistance(x,z) -> Mth.chessboardDistance(x,z,this.x,this.z)
//                                = Math.max(Math.abs(x-this.x), Math.abs(z-this.z))
//   distanceSquared(x,z)       -> dx*dx + dz*dz  (private helper)
//
// Helper sources:
//   26.1.2/src/net/minecraft/core/SectionPos.java
//     public static int sectionToBlockCoord(final int sectionCoord)              { return sectionCoord << 4; }
//     public static int sectionToBlockCoord(final int sectionCoord, int offset)  { return (sectionCoord << 4) + offset; }
//   26.1.2/src/net/minecraft/util/Mth.java
//     public static int absMax(final int a, final int b)                         { return Math.max(Math.abs(a), Math.abs(b)); }
//     public static int chessboardDistance(int x0,int z0,int x1,int z1)          { return absMax(x1 - x0, z1 - z0); }
//
// All math is on Java `int` (int32_t). Java `<<`/`>>`/`&` on int are well
// defined; we mirror them with int32_t arithmetic. Shift counts here are small
// constants (4 and 5) so no masking subtleties arise. `getChessboardDistance`
// and `distanceSquared` use ordinary signed int arithmetic that may overflow
// exactly as Java does (2's complement wraparound), which is intentional and
// matched bit-for-bit by int32_t in C++.
//
// NOTE: ChunkPos packing/unpacking, hashing, isValid, CODEC/STREAM_CODEC,
// rangeClosed, containing, BlockPos producers (getBlockAt/getWorldPosition/
// getMiddleBlockPosition/contains) are intentionally NOT ported here — packing
// lives elsewhere, and the rest depend on un-ported types. See unportedMethods.

#include <cstdint>

namespace mc::chunkpos {

// SectionPos.sectionToBlockCoord(int) = sectionCoord << 4
constexpr int32_t sectionToBlockCoord(int32_t sectionCoord) noexcept {
    return sectionCoord << 4;
}

// SectionPos.sectionToBlockCoord(int, int) = (sectionCoord << 4) + offset
constexpr int32_t sectionToBlockCoord(int32_t sectionCoord, int32_t offset) noexcept {
    return (sectionCoord << 4) + offset;
}

// Mth.absMax(int,int) = Math.max(Math.abs(a), Math.abs(b))
// Java Math.abs(int) of INT_MIN returns INT_MIN (no UB); std::abs would be UB.
// Mirror Java exactly with a branch that leaves INT_MIN unchanged.
constexpr int32_t javaAbs(int32_t a) noexcept {
    return a < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(a)) : a;
}
constexpr int32_t absMax(int32_t a, int32_t b) noexcept {
    int32_t aa = javaAbs(a);
    int32_t bb = javaAbs(b);
    return aa > bb ? aa : bb;
}

// ChunkPos.getBlockX(offset) = SectionPos.sectionToBlockCoord(this.x, offset)
constexpr int32_t getBlockX(int32_t x, int32_t offset) noexcept {
    return sectionToBlockCoord(x, offset);
}
// ChunkPos.getBlockZ(offset) = SectionPos.sectionToBlockCoord(this.z, offset)
constexpr int32_t getBlockZ(int32_t z, int32_t offset) noexcept {
    return sectionToBlockCoord(z, offset);
}

// ChunkPos.getMinBlockX() = SectionPos.sectionToBlockCoord(this.x)
constexpr int32_t getMinBlockX(int32_t x) noexcept {
    return sectionToBlockCoord(x);
}
// ChunkPos.getMinBlockZ() = SectionPos.sectionToBlockCoord(this.z)
constexpr int32_t getMinBlockZ(int32_t z) noexcept {
    return sectionToBlockCoord(z);
}

// ChunkPos.getMaxBlockX() = getBlockX(15)
constexpr int32_t getMaxBlockX(int32_t x) noexcept {
    return getBlockX(x, 15);
}
// ChunkPos.getMaxBlockZ() = getBlockZ(15)
constexpr int32_t getMaxBlockZ(int32_t z) noexcept {
    return getBlockZ(z, 15);
}

// ChunkPos.getMiddleBlockX() = getBlockX(8)
constexpr int32_t getMiddleBlockX(int32_t x) noexcept {
    return getBlockX(x, 8);
}
// ChunkPos.getMiddleBlockZ() = getBlockZ(8)
constexpr int32_t getMiddleBlockZ(int32_t z) noexcept {
    return getBlockZ(z, 8);
}

// ChunkPos.getRegionX() = this.x >> 5
constexpr int32_t getRegionX(int32_t x) noexcept {
    return x >> 5;
}
// ChunkPos.getRegionZ() = this.z >> 5
constexpr int32_t getRegionZ(int32_t z) noexcept {
    return z >> 5;
}

// ChunkPos.getRegionLocalX() = this.x & 31
constexpr int32_t getRegionLocalX(int32_t x) noexcept {
    return x & 31;
}
// ChunkPos.getRegionLocalZ() = this.z & 31
constexpr int32_t getRegionLocalZ(int32_t z) noexcept {
    return z & 31;
}

// ChunkPos.getChessboardDistance(int x, int z) on a ChunkPos(thisX,thisZ):
//   return Mth.chessboardDistance(x, z, this.x, this.z)
//        = absMax(this.x - x, this.z - z)
// (Mth.chessboardDistance(x0,z0,x1,z1) = absMax(x1-x0, z1-z0), here x0=x,z0=z,x1=thisX,z1=thisZ)
constexpr int32_t getChessboardDistance(int32_t thisX, int32_t thisZ,
                                        int32_t x, int32_t z) noexcept {
    int32_t dx = static_cast<int32_t>(static_cast<uint32_t>(thisX) - static_cast<uint32_t>(x));
    int32_t dz = static_cast<int32_t>(static_cast<uint32_t>(thisZ) - static_cast<uint32_t>(z));
    return absMax(dx, dz);
}

// ChunkPos.distanceSquared(int x, int z) on a ChunkPos(thisX,thisZ):
//   int deltaX = x - this.x; int deltaZ = z - this.z;
//   return deltaX * deltaX + deltaZ * deltaZ;
// Signed int arithmetic with 2's-complement wraparound, matched by uint casts.
constexpr int32_t distanceSquared(int32_t thisX, int32_t thisZ,
                                  int32_t x, int32_t z) noexcept {
    int32_t deltaX = static_cast<int32_t>(static_cast<uint32_t>(x) - static_cast<uint32_t>(thisX));
    int32_t deltaZ = static_cast<int32_t>(static_cast<uint32_t>(z) - static_cast<uint32_t>(thisZ));
    uint32_t sq = static_cast<uint32_t>(deltaX) * static_cast<uint32_t>(deltaX)
                + static_cast<uint32_t>(deltaZ) * static_cast<uint32_t>(deltaZ);
    return static_cast<int32_t>(sq);
}

} // namespace mc::chunkpos
