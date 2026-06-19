#pragma once
// 1:1 port of net.minecraft.world.InteractionHand (Minecraft Java Edition 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/InteractionHand.java
//
// A bare two-constant enum:
//   MAIN_HAND(0), OFF_HAND(1)   (InteractionHand.java:10-12)
// carrying one private `id` field set to {0, 1} from the ctor argument
// (InteractionHand.java:11-12, 18-20). The id happens to equal the ordinal.
//
// Ported here verbatim from the Java:
//   - ordinals / declaration order             (InteractionHand.java:10-12)
//   - the private `id` field                   (InteractionHand.java:11,12,16-20)
//   - BY_ID = ByIdMap.continuous(h -> h.id, values(), OutOfBoundsStrategy.ZERO)
//                                               (InteractionHand.java:14)
//     ByIdMap.continuous/createSortedArray/ZERO (util/ByIdMap.java:34-78):
//     ids are 0..1, continuous; sorted[id] = the constant with that id, so
//     sorted = {MAIN_HAND, OFF_HAND}; ZERO yields:
//       id in [0,2) -> sorted[id], else sorted[0] (== MAIN_HAND, the id==0 value).
//   - asEquipmentSlot()                         (InteractionHand.java:22-24):
//       this == MAIN_HAND ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND.
//     We return the EquipmentSlot ORDINAL (MAINHAND=0, OFFHAND=1 per
//     EquipmentSlot.java:13-14 / world/entity/EquipmentSlotData.h) so the
//     mapping is verifiable without pulling in that header.
//
// NOT ported (registry/codec/network coupled — deliberately ABSENT, not faked):
//   - STREAM_CODEC (ByteBufCodecs.idMapper) — network codec machinery. The
//     BEHAVIOR of its underlying BY_ID mapper IS reproduced (byId below).

#include <cstdint>

namespace mc {

// InteractionHand enum constants, declaration order (InteractionHand.java:10-12).
enum class InteractionHand : int32_t {
    MAIN_HAND = 0,
    OFF_HAND  = 1,
};

inline constexpr int INTERACTION_HAND_COUNT = 2;

// The private `id` field — the sole ctor argument (InteractionHand.java:11-12).
// MAIN_HAND -> 0, OFF_HAND -> 1.
inline constexpr int interactionHandId(InteractionHand h) noexcept {
    return static_cast<int>(h);
}

// BY_ID — ByIdMap.continuous(h -> h.id, values(), OutOfBoundsStrategy.ZERO)
// (InteractionHand.java:14; util/ByIdMap.java:66-78).
// sorted array indexed by id (ids are 0,1 and continuous):
//   sorted[0] = MAIN_HAND, sorted[1] = OFF_HAND.
// ZERO strategy (ByIdMap.java:71-73): in-range -> sorted[id]; else sorted[0].
inline constexpr InteractionHand SORTED_BY_ID[INTERACTION_HAND_COUNT] = {
    InteractionHand::MAIN_HAND, // id 0
    InteractionHand::OFF_HAND,  // id 1
};

inline constexpr InteractionHand interactionHandById(int id) noexcept {
    if (id >= 0 && id < INTERACTION_HAND_COUNT) return SORTED_BY_ID[id];
    return SORTED_BY_ID[0]; // ZERO fallback (sorted[0] == MAIN_HAND, the id==0 value)
}

// asEquipmentSlot() — InteractionHand.java:22-24.
//   this == MAIN_HAND ? EquipmentSlot.MAINHAND : EquipmentSlot.OFFHAND.
// Returned as the EquipmentSlot ordinal: MAINHAND=0, OFFHAND=1
// (EquipmentSlot.java:13-14 / world/entity/EquipmentSlotData.h Slot enum).
inline constexpr int interactionHandAsEquipmentSlotOrdinal(InteractionHand h) noexcept {
    return h == InteractionHand::MAIN_HAND ? 0 /*MAINHAND*/ : 1 /*OFFHAND*/;
}

} // namespace mc
