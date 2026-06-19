// 1:1 port of net.minecraft.world.entity.animal.rabbit.Rabbit.Variant
// (Minecraft 26.1.2)
//
// Pure, world-free data enum: 7 rabbit coat variants, each carrying an int id
// and a serialized name. The interesting part is byId(), which is backed by
// ByIdMap.sparse(...).
//
// Source: 26.1.2/src/net/minecraft/world/entity/animal/rabbit/Rabbit.java
//           public enum Variant implements StringRepresentable {
//              BROWN(0, "brown"), WHITE(1, "white"), BLACK(2, "black"),
//              WHITE_SPLOTCHED(3, "white_splotched"), GOLD(4, "gold"),
//              SALT(5, "salt"), EVIL(99, "evil");
//              public static final Variant DEFAULT = BROWN;
//              private static final IntFunction<Variant> BY_ID =
//                 ByIdMap.sparse(Variant::id, values(), DEFAULT);
//              public static Variant byId(int id) { return BY_ID.apply(id); }
//           }
//
//         26.1.2/src/net/minecraft/util/ByIdMap.java
//           static <T> IntFunction<T> sparse(ToIntFunction<T> idGetter,
//                                            T[] values, T _default) {
//              IntFunction<T> idToObject = createMap(idGetter, values);
//              return id -> Objects.requireNonNullElse(idToObject.apply(id), _default);
//           }
//           // createMap builds an Int2ObjectOpenHashMap keyed by id(value).
//
// TRAP: ByIdMap.sparse is a *hash map keyed by the declared id*, NOT a
// contiguous-array lookup. The declared ids are {0,1,2,3,4,5,99}. ANY id that
// is not a declared key -- negatives, the holes 6..98, and 100+ -- folds to the
// _default value (BROWN). This is distinct from ByIdMap.continuous, where the
// array index is the *position* in a 0..n-1 dense array; here EVIL keeps its
// real id 99 and there is no dense array. We mirror that exactly: a key lookup
// against the seven declared ids, default BROWN on miss.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mc::world::entity::animal::rabbit {

// Enum ordinals, in declaration order (== Java Enum.ordinal()).
enum class RabbitVariant : std::int32_t {
   BROWN = 0,
   WHITE = 1,
   BLACK = 2,
   WHITE_SPLOTCHED = 3,
   GOLD = 4,
   SALT = 5,
   EVIL = 6,  // ordinal 6; its DECLARED id() is 99 (see kId below)
};

inline constexpr int kRabbitVariantCount = 7;

// DEFAULT = BROWN (returned by byId on any unmapped id).
inline constexpr RabbitVariant kRabbitVariantDefault = RabbitVariant::BROWN;

// Per-ordinal declared id() value. Note EVIL = 99, NOT 6.
inline constexpr std::array<std::int32_t, kRabbitVariantCount> kRabbitVariantId{
    0, 1, 2, 3, 4, 5, 99};

// Per-ordinal getSerializedName().
inline constexpr std::array<std::string_view, kRabbitVariantCount>
    kRabbitVariantName{"brown",           "white", "black", "white_splotched",
                       "gold",            "salt",  "evil"};

// Rabbit.Variant.id()
inline constexpr std::int32_t rabbitVariantId(RabbitVariant v) {
   return kRabbitVariantId[static_cast<std::size_t>(v)];
}

// Rabbit.Variant.getSerializedName()
inline constexpr std::string_view rabbitVariantSerializedName(RabbitVariant v) {
   return kRabbitVariantName[static_cast<std::size_t>(v)];
}

// Rabbit.Variant.byId(int) == ByIdMap.sparse(Variant::id, values(), DEFAULT).
//
// Faithful semantics: the backing structure is an Int2ObjectOpenHashMap keyed
// by the declared id(); apply(id) returns the value whose id() == id, or null;
// Objects.requireNonNullElse then substitutes DEFAULT (BROWN) on null. We scan
// the seven declared ids (a hash map over 7 distinct int keys; iteration order
// is irrelevant because the keys are distinct and we return on the unique
// match) and fall back to DEFAULT.
inline constexpr RabbitVariant rabbitVariantById(std::int32_t id) {
   for (int ordinal = 0; ordinal < kRabbitVariantCount; ++ordinal) {
      if (kRabbitVariantId[static_cast<std::size_t>(ordinal)] == id) {
         return static_cast<RabbitVariant>(ordinal);
      }
   }
   return kRabbitVariantDefault;
}

}  // namespace mc::world::entity::animal::rabbit
