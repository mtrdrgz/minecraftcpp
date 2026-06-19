#pragma once
#include <algorithm>
#include <cstdint>

#include "Vec3i.h"  // mc::Vec3i (certified int arithmetic), mc::Direction, DIRECTION_NORMAL, directionAxisDirection

// ---------------------------------------------------------------------------
// Port of the pure-integer surface of net/minecraft/core/BlockBox.java
// (Minecraft Java Edition 26.1.2).
//
// BlockBox is `record BlockBox(BlockPos min, BlockPos max)`. The canonical
// constructor NORMALIZES so min/max are the per-component min/max of the two
// args:
//     this.min = BlockPos.min(min, max);   // BlockBox.java:22
//     this.max = BlockPos.max(min, max);   // BlockBox.java:23
//
// BlockPos extends Vec3i, and every operation BlockBox uses (min/max/relative/
// offset) is the SAME integer arithmetic that the certified mc::Vec3i already
// implements. So we reuse mc::Vec3i as the corner type — its relative()/offset()
// are bit-identical to BlockPos.relative()/offset() (BlockPos.java:122-124,
// 134-136, 202-206 mirror Vec3i.java exactly).
//
// BlockPos.min(a,b) — BlockPos.java:99-101  -> per-component Math.min.
// BlockPos.max(a,b) — BlockPos.java:103-105 -> per-component Math.max.
//
// Ported (pure int — exchanged with BlockBoxParity.java):
//   BlockBox(min,max) canonical ctor    BlockBox.java:21-24
//   of(pos)                             BlockBox.java:26-28
//   of(a,b)                             BlockBox.java:30-32
//   include(pos)  ("encapsulate")       BlockBox.java:34-36
//   isBlock()                           BlockBox.java:38-40
//   contains(pos) ("isInside")          BlockBox.java:42-49
//   sizeX()/sizeY()/sizeZ()             BlockBox.java:60-70
//   extend(dir, amount)                 BlockBox.java:72-80
//   move(dir, amount)                   BlockBox.java:82-84
//   offset(Vec3i)                       BlockBox.java:86-88
//
// NOT ported (out of the pure-int scope; hard-absent, NOT stubbed):
//   aabb()         — needs net.minecraft.world.phys.AABB (BlockBox.java:51-53)
//   iterator()     — needs BlockPos.betweenClosed iteration (BlockBox.java:55-58)
//   STREAM_CODEC   — needs FriendlyByteBuf/network (BlockBox.java:10-19)
//
// NOTE: the assignment's method list mentioned "infinite/intersects/getCenter".
// net.minecraft.core.BlockBox (26.1.2) has NONE of those — they belong to the
// legacy structure BoundingBox class, which is a different type. Only the methods
// that actually exist on core.BlockBox are ported here.
//
// Java int == int32_t; all arithmetic here is int subtraction/addition which can
// wrap two's-complement. We route the size (max-min+1) through the same uint
// helpers mc::Vec3i uses so extremes match Java bit-for-bit.
// ---------------------------------------------------------------------------

namespace mc {

struct BlockBox {
    Vec3i min;  // BlockBox.min — always the per-component minimum corner.
    Vec3i max;  // BlockBox.max — always the per-component maximum corner.

    // Java: BlockPos.min(a,b) — per-component Math.min (BlockPos.java:99-101).
    static constexpr Vec3i posMin(const Vec3i& a, const Vec3i& b) noexcept {
        return Vec3i(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
    }
    // Java: BlockPos.max(a,b) — per-component Math.max (BlockPos.java:103-105).
    static constexpr Vec3i posMax(const Vec3i& a, const Vec3i& b) noexcept {
        return Vec3i(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
    }

    // Java: canonical ctor BlockBox(min,max) — normalizes (BlockBox.java:21-24).
    constexpr BlockBox(const Vec3i& a, const Vec3i& b) noexcept
        : min(posMin(a, b)), max(posMax(a, b)) {}

    constexpr bool operator==(const BlockBox&) const = default;

    // Java: of(pos) — new BlockBox(pos,pos) (BlockBox.java:26-28).
    static constexpr BlockBox of(const Vec3i& pos) noexcept { return BlockBox(pos, pos); }
    // Java: of(a,b) — new BlockBox(a,b) (BlockBox.java:30-32).
    static constexpr BlockBox of(const Vec3i& a, const Vec3i& b) noexcept { return BlockBox(a, b); }

    // Java: include(pos) — new BlockBox(BlockPos.min(min,pos), BlockPos.max(max,pos))
    // (BlockBox.java:34-36). The result is re-normalized by the ctor.
    constexpr BlockBox include(const Vec3i& pos) const noexcept {
        return BlockBox(posMin(min, pos), posMax(max, pos));
    }

    // Java: isBlock() — min.equals(max) (BlockBox.java:38-40).
    constexpr bool isBlock() const noexcept { return min == max; }

    // Java: contains(pos) — closed-interval containment on all 3 axes
    // (BlockBox.java:42-49).
    constexpr bool contains(const Vec3i& pos) const noexcept {
        return pos.getX() >= min.getX() && pos.getY() >= min.getY() && pos.getZ() >= min.getZ()
            && pos.getX() <= max.getX() && pos.getY() <= max.getY() && pos.getZ() <= max.getZ();
    }

    // Java: sizeX/sizeY/sizeZ — max - min + 1 (BlockBox.java:60-70). int arithmetic
    // (subtract then +1) wraps two's-complement; replicate via uint like mc::Vec3i.
    constexpr int32_t sizeX() const noexcept { return iAdd(iSub(max.x, min.x), 1); }
    constexpr int32_t sizeY() const noexcept { return iAdd(iSub(max.y, min.y), 1); }
    constexpr int32_t sizeZ() const noexcept { return iAdd(iSub(max.z, min.z), 1); }

    // Java: extend(direction, amount) — BlockBox.java:72-80.
    //   amount==0 -> this;
    //   POSITIVE axisDirection -> of(min, BlockPos.max(min, max.relative(dir,amount)));
    //   NEGATIVE axisDirection -> of(BlockPos.min(min.relative(dir,amount), max), max).
    // NOTE: relative() is non-constexpr in mc::Vec3i (table-free but member-fn).
    BlockBox extend(Direction direction, int32_t amount) const noexcept {
        if (amount == 0) return *this;
        if (directionAxisDirection(direction) == AxisDirection::POSITIVE) {
            return of(min, posMax(min, max.relative(direction, amount)));
        }
        return of(posMin(min.relative(direction, amount), max), max);
    }

    // Java: move(direction, amount) — BlockBox.java:82-84.
    //   amount==0 -> this; else new BlockBox(min.relative(d,amount), max.relative(d,amount)).
    BlockBox move(Direction direction, int32_t amount) const noexcept {
        if (amount == 0) return *this;
        return BlockBox(min.relative(direction, amount), max.relative(direction, amount));
    }

    // Java: offset(Vec3i) — new BlockBox(min.offset(offset), max.offset(offset))
    // (BlockBox.java:86-88).
    constexpr BlockBox offset(const Vec3i& off) const noexcept {
        return BlockBox(min.offset(off), max.offset(off));
    }

private:
    // Java int +/- wraparound (two's complement), matching mc::Vec3i's helpers.
    static constexpr int32_t iAdd(int32_t a, int32_t b) noexcept {
        return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    }
    static constexpr int32_t iSub(int32_t a, int32_t b) noexcept {
        return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
    }
};

} // namespace mc
