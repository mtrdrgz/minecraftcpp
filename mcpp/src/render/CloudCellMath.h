// Bit-exact 1:1 C++ port of the pure cell/geometry math inside
// net.minecraft.client.renderer.CloudRenderer (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/client/renderer/CloudRenderer.java
//
// CloudRenderer itself owns GPU buffers (MappableRingBuffer, UBOs) and issues real
// render passes, BUT the parts ported here are entirely pure — no GL/GPU/window
// state — and are exercised in isolation:
//
//   * the cell pack/unpack bit layout: packCellData(), isCellEmpty(),
//     isNorth/East/South/WestEmpty()  (CloudRenderer.java:125-147, 129-131)
//   * getSizeForCloudDistance(int) — the index/face capacity formula
//     (CloudRenderer.java:113-118)
//   * the per-face byte encoding from encodeFace() — the (dir | flags | parity-bits)
//     packing and the >>1 packed x/z bytes (CloudRenderer.java:360-365)
//   * the per-frame cloud-cell positioning math from render() — the wrap of the
//     scrolled cloud position into texture space, the integer cell coords, and the
//     fractional in-cell offsets (CloudRenderer.java:181-191)
//
// ── 1:1 traps faithfully reproduced ─────────────────────────────────────────
//   * packCellData / isCellEmpty operate on a Java `long` (64-bit); we mirror with
//     int64_t. `(long)color << 4` sign-extends the 32-bit color before shifting —
//     reproduced by casting color to int64_t first (matches Java widening).
//   * isCellEmpty uses ARGB.alpha(color) (unsigned >>24) < 10. Routed through the
//     gated mc::argb::alpha so it stays identical.
//   * encodeFace mixes int bit-ops with NARROWING to signed `byte`: the three
//     emitted bytes are `(byte)(x>>1)`, `(byte)(z>>1)`, `(byte)dirAndFlags`. Java
//     `>>` is arithmetic (sign-preserving) on int; the (byte) cast truncates to the
//     low 8 bits as a two's-complement signed value. We return int8_t to capture the
//     exact truncated bit pattern (e.g. dirAndFlags with bit7 set => negative byte).
//   * The cloud-cell math is a careful float/double dance:
//       cloudOffset = (float)(gameTime % (width*400L)) + partialTicks   // float
//       cloudX (double) = camX + cloudOffset * 0.030000001F             // float lit
//       cloudZ (double) = camZ + 3.96F
//       cloudX -= Mth.floor(cloudX / texWBlocks) * texWBlocks           // double
//       cellX  = Mth.floor(cloudX / 12.0)                               // int
//       xInCell = (float)(cloudX - cellX * 12.0F)   // NB: 12.0F (float) in the
//                                                   //     subtraction, not 12.0
//     The `* 0.030000001F` and `12.0F` are float literals promoted to double inside
//     the double expressions; `cellX * 12.0F` is int*float => double here because the
//     whole RHS of the (double) cast is double. We replicate the exact literal types
//     and the (float) narrowing on the final in-cell offsets. ffp-contract=off keeps
//     a*b+c un-fused, matching Java (no FMA).
//
// Verbatim Java reference (the ported fragments):
//
//   private static boolean isCellEmpty(int color) { return ARGB.alpha(color) < 10; }
//   private static long packCellData(int color, boolean n, boolean e, boolean s, boolean w) {
//       return (long)color << 4 | (n?1:0)<<3 | (e?1:0)<<2 | (s?1:0)<<1 | (w?1:0)<<0; }
//   private static boolean isNorthEmpty(long c){ return (c>>3 & 1L)!=0L; }
//   private static boolean isEastEmpty (long c){ return (c>>2 & 1L)!=0L; }
//   private static boolean isSouthEmpty(long c){ return (c>>1 & 1L)!=0L; }
//   private static boolean isWestEmpty (long c){ return (c>>0 & 1L)!=0L; }
//   private static int getSizeForCloudDistance(int radiusCells){
//       int maxCells = (radiusCells+1)*2 * (radiusCells+1)*2 / 2;
//       int maxFaces = maxCells*4 + 54; return maxFaces*3; }
//   // encodeFace: dirAndFlags = dir.get3DDataValue() | flags;
//   //             dirAndFlags |= (x&1)<<7; dirAndFlags |= (z&1)<<6;
//   //             put((byte)(x>>1)).put((byte)(z>>1)).put((byte)dirAndFlags);

#pragma once

#include <cstdint>

#include "util/ARGB.h"
#include "world/level/levelgen/Mth.h"

namespace mc::render::cloud {

namespace mth = mc::levelgen::mth;

// ── constants verbatim from CloudRenderer.java ──────────────────────────────
inline constexpr float CELL_SIZE_IN_BLOCKS = 12.0F;  // line 42
inline constexpr int TICKS_PER_CELL = 400;           // line 43

// Direction.get3DDataValue() — Direction.java first ctor arg.
// DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
enum class Dir3D : int { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// CloudRenderer.java:125-127 — ARGB.alpha(color) < 10.
inline bool isCellEmpty(int color) { return mc::argb::alpha(color) < 10; }

// CloudRenderer.java:129-131. `(long)color << 4` sign-extends color first.
inline int64_t packCellData(int color, bool north, bool east, bool south, bool west) {
    return (static_cast<int64_t>(color) << 4)
         | (static_cast<int64_t>(north ? 1 : 0) << 3)
         | (static_cast<int64_t>(east  ? 1 : 0) << 2)
         | (static_cast<int64_t>(south ? 1 : 0) << 1)
         | (static_cast<int64_t>(west  ? 1 : 0) << 0);
}

// CloudRenderer.java:133-147.
inline bool isNorthEmpty(int64_t c) { return ((c >> 3) & 1LL) != 0LL; }
inline bool isEastEmpty (int64_t c) { return ((c >> 2) & 1LL) != 0LL; }
inline bool isSouthEmpty(int64_t c) { return ((c >> 1) & 1LL) != 0LL; }
inline bool isWestEmpty (int64_t c) { return ((c >> 0) & 1LL) != 0LL; }

// CloudRenderer.java:113-118 — index/face capacity (pure int32; mirrors any wrap).
inline int32_t getSizeForCloudDistance(int32_t radiusCells) {
    int32_t maxCells = (radiusCells + 1) * 2 * (radiusCells + 1) * 2 / 2;
    int32_t maxFaces = maxCells * 4 + 54;
    return maxFaces * 3;
}

// CloudRenderer.java:360-364 — the int dirAndFlags before narrowing.
inline int32_t faceDirAndFlags(Dir3D dir, int32_t flags, int32_t x, int32_t z) {
    int32_t dirAndFlags = static_cast<int32_t>(dir) | flags;
    dirAndFlags |= (x & 1) << 7;
    dirAndFlags |= (z & 1) << 6;
    return dirAndFlags;
}

// The three bytes encodeFace() writes: (byte)(x>>1), (byte)(z>>1), (byte)dirAndFlags.
// Java `>>` is arithmetic on int; the (byte) cast keeps the low 8 bits, signed.
struct EncodedFace { int8_t bx; int8_t bz; int8_t bDirFlags; };
inline EncodedFace encodeFace(Dir3D dir, int32_t flags, int32_t x, int32_t z) {
    int32_t dirAndFlags = faceDirAndFlags(dir, flags, x, z);
    return EncodedFace{
        static_cast<int8_t>(x >> 1),
        static_cast<int8_t>(z >> 1),
        static_cast<int8_t>(dirAndFlags),
    };
}

// CloudRenderer.java:181-191 — the per-frame cloud-cell positioning math.
// width/height are the cloud texture dimensions (this.texture.{width,height}).
struct CloudCellPos {
    double cloudX;
    double cloudZ;
    int32_t cellX;
    int32_t cellZ;
    float xInCell;
    float zInCell;
};
inline CloudCellPos computeCloudCellPos(int width, int height, double cameraX,
                                        double cameraZ, int64_t gameTime,
                                        float partialTicks) {
    // (float)(gameTime % (width * 400L)) + partialTicks  — result is float.
    float cloudOffset =
        static_cast<float>(gameTime % (static_cast<int64_t>(width) * 400LL)) + partialTicks;
    // double = camX + cloudOffset * 0.030000001F (float literal -> double in expr)
    double cloudX = cameraX + cloudOffset * 0.030000001F;
    double cloudZ = cameraZ + 3.96F;
    double textureWidthBlocks = width * 12.0;
    double textureHeightBlocks = height * 12.0;
    cloudX -= mth::floor(cloudX / textureWidthBlocks) * textureWidthBlocks;
    cloudZ -= mth::floor(cloudZ / textureHeightBlocks) * textureHeightBlocks;
    int32_t cellX = mth::floor(cloudX / 12.0);
    int32_t cellZ = mth::floor(cloudZ / 12.0);
    // NB: 12.0F (float) in the in-cell subtraction; whole RHS is double then (float).
    float xInCell = static_cast<float>(cloudX - cellX * 12.0F);
    float zInCell = static_cast<float>(cloudZ - cellZ * 12.0F);
    return CloudCellPos{cloudX, cloudZ, cellX, cellZ, xInCell, zInCell};
}

}  // namespace mc::render::cloud
