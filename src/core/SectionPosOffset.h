#pragma once

// 1:1 port of the net.minecraft.core.SectionPos *relative-offset* surface that the
// existing certified codec header (core/PosCodec.h, namespace mc::poscodec) does NOT
// yet cover. Everything else SectionPos exposes — asLong(x,y,z), x()/y()/z(),
// blockToSectionCoord, sectionToBlockCoord, sectionRelative* — already lives in
// PosCodec.h (certified by pos_codec_parity) and is REUSED, not duplicated, here.
//
// This header adds ONLY the packed-long "relative" / offset operations:
//   SectionPos.offset(long sectionNode, int stepX, int stepY, int stepZ)  — SectionPos.java:72-74
//   SectionPos.offset(long sectionNode, Direction)                        — SectionPos.java:68-70
//   SectionPos.SectionPos.offset(int,int,int) (instance, the {x,y,z} form) — SectionPos.java:227-229
//
// SectionPos.java:72-74:
//   public static long offset(long sectionNode, int stepX, int stepY, int stepZ) {
//      return asLong(x(sectionNode) + stepX, y(sectionNode) + stepY, z(sectionNode) + stepZ);
//   }
// SectionPos.java:68-70:
//   public static long offset(long sectionNode, Direction offset) {
//      return offset(sectionNode, offset.getStepX(), offset.getStepY(), offset.getStepZ());
//   }
// SectionPos.java:227-229 (instance):
//   public SectionPos offset(int x, int y, int z) {
//      return x==0 && y==0 && z==0 ? this : new SectionPos(x()+x, y()+y, z()+z);
//   }
//
// All arithmetic is Java two's-complement int +/+; the unpack→add→repack round-trips
// the 22/20/22-bit fields exactly. Certified bit-exact by section_pos_parity.

#include <cstdint>

#include "core/PosCodec.h"        // mc::poscodec::sectionPosAsLong / sectionPosX/Y/Z
#include "world/phys/Direction.h" // mc::DIRECTION_NORMAL (Direction.getStepX/Y/Z)

namespace mc::sectionpos {

// SectionPos.offset(long, int, int, int) — SectionPos.java:72-74.
inline int64_t offset(int64_t sectionNode, int stepX, int stepY, int stepZ) {
    return poscodec::sectionPosAsLong(poscodec::sectionPosX(sectionNode) + stepX,
                                      poscodec::sectionPosY(sectionNode) + stepY,
                                      poscodec::sectionPosZ(sectionNode) + stepZ);
}

// SectionPos.offset(long, Direction) — SectionPos.java:68-70.
// offset.getStepX/Y/Z() == Direction.normal.getX/Y/Z() (Direction.java:255-265),
// which is exactly DIRECTION_NORMAL[ordinal][0..2].
inline int64_t offset(int64_t sectionNode, Direction dir) {
    const int o = static_cast<int>(dir);
    return offset(sectionNode, DIRECTION_NORMAL[o][0], DIRECTION_NORMAL[o][1], DIRECTION_NORMAL[o][2]);
}

// SectionPos instance offset(int,int,int) — SectionPos.java:227-229. The (0,0,0)
// short-circuit returns `this` in Java; on the {x,y,z} value it is a pure identity,
// so the resulting coordinates are identical either way. We model it on the
// section-coordinate triple (not the packed long) to mirror the field-wise form.
struct SectionCoord {
    int32_t x, y, z;
    constexpr bool operator==(const SectionCoord&) const = default;
};

inline SectionCoord offsetCoord(SectionCoord s, int x, int y, int z) {
    if (x == 0 && y == 0 && z == 0) return s; // SectionPos.java:228 short-circuit
    return SectionCoord{s.x + x, s.y + y, s.z + z};
}

} // namespace mc::sectionpos
