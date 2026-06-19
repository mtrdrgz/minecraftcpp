// Bit-exact 1:1 C++ port of com.mojang.blaze3d.buffers.Std140SizeCalculator
// (Minecraft 26.1.2).
//
// Source: 26.1.2/src/com/mojang/blaze3d/buffers/Std140SizeCalculator.java
//
// A tiny stateful builder that computes the byte size of a std140 UBO layout. You
// chain put<Type>() calls in declaration order and read the total with get(). Each
// put aligns the running offset up to that type's std140 base alignment, then adds
// the type's std140 size. Pure 32-bit integer arithmetic — no GL/GPU/window state
// whatsoever (the real GL upload lives in the sibling Std140Builder; this class
// only sizes the buffer). The renderer uses it to size UBOs at class-init time,
// e.g. FogRenderer.FOG_UBO_SIZE, Lightmap.LIGHTMAP_UBO_SIZE, CloudRenderer.UBO_SIZE.
//
// ── std140 alignment/size table (verbatim from the Java) ─────────────────────
//   align/size  type
//   4 / 4       float, int                 (putFloat, putInt)
//   8 / 8       vec2, ivec2                (putVec2, putIVec2)
//   16 / 16     vec3, ivec3, vec4, ivec4   (putVec3, putIVec3, putVec4, putIVec4)
//   16 / 64     mat4                       (putMat4f)  (4 columns x vec4, 16 each)
//
// Note std140's well-known quirk, reproduced here because the Java does it: vec3
// has a *base alignment* of 16 and the class also advances size by 16 for it (it
// does NOT pack a trailing scalar into the vec3's padding — a following putFloat
// re-aligns to 4, which is already satisfied, and then adds 4). vec3 followed by
// float thus occupies 16 + 4 = 20 bytes, etc.
//
// ── 1:1 traps faithfully reproduced ─────────────────────────────────────────
//   * align(a) uses net.minecraft.util.Mth.roundToward(size, a), which is
//     `positiveCeilDiv(size, a) * a` = `-Math.floorDiv(-size, a) * a`. For the
//     non-negative sizes produced here it equals the naive round-up, but we route
//     through the same floorDiv-based formula so it stays bit-identical even if
//     ever called on a different running value (NOT the truncating
//     `(size + a - 1) / a * a`, which differs for negatives / near overflow).
//   * size is a plain `int` (32-bit). All arithmetic is int32; we mirror that with
//     int32_t so any wrap matches Java two's-complement.
//   * Builder methods mutate `this.size` in place and return *this for chaining;
//     get() returns the current accumulated size.
//
// Verbatim Java reference (Std140SizeCalculator.java):
//
//   private int size;
//   public int get()                              { return this.size; }
//   public Std140SizeCalculator align(int a)      { this.size = Mth.roundToward(this.size, a); return this; }
//   public Std140SizeCalculator putFloat()        { this.align(4);  this.size += 4;  return this; }
//   public Std140SizeCalculator putInt()          { this.align(4);  this.size += 4;  return this; }
//   public Std140SizeCalculator putVec2()         { this.align(8);  this.size += 8;  return this; }
//   public Std140SizeCalculator putIVec2()        { this.align(8);  this.size += 8;  return this; }
//   public Std140SizeCalculator putVec3()         { this.align(16); this.size += 16; return this; }
//   public Std140SizeCalculator putIVec3()        { this.align(16); this.size += 16; return this; }
//   public Std140SizeCalculator putVec4()         { this.align(16); this.size += 16; return this; }
//   public Std140SizeCalculator putIVec4()        { this.align(16); this.size += 16; return this; }
//   public Std140SizeCalculator putMat4f()        { this.align(16); this.size += 64; return this; }

#pragma once

#include <cstdint>

#include "world/level/levelgen/Mth.h"

namespace mc::render {

class Std140SizeCalculator {
public:
    // `int get()` — the accumulated std140 size in bytes.
    int32_t get() const { return size_; }

    // `Std140SizeCalculator align(int alignment)` — Mth.roundToward(size, a).
    Std140SizeCalculator& align(int32_t alignment) {
        size_ = mc::levelgen::mth::roundToward(size_, alignment);
        return *this;
    }

    Std140SizeCalculator& putFloat() { align(4);  size_ += 4;  return *this; }
    Std140SizeCalculator& putInt()   { align(4);  size_ += 4;  return *this; }
    Std140SizeCalculator& putVec2()  { align(8);  size_ += 8;  return *this; }
    Std140SizeCalculator& putIVec2() { align(8);  size_ += 8;  return *this; }
    Std140SizeCalculator& putVec3()  { align(16); size_ += 16; return *this; }
    Std140SizeCalculator& putIVec3() { align(16); size_ += 16; return *this; }
    Std140SizeCalculator& putVec4()  { align(16); size_ += 16; return *this; }
    Std140SizeCalculator& putIVec4() { align(16); size_ += 16; return *this; }
    Std140SizeCalculator& putMat4f() { align(16); size_ += 64; return *this; }

private:
    int32_t size_ = 0;  // Java `private int size;` (default 0)
};

}  // namespace mc::render
