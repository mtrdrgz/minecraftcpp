#pragma once
// Port of net.minecraft.world.entity.player.ChatVisiblity (MC 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/entity/player/ChatVisiblity.java):
//   public enum ChatVisiblity {
//      FULL(0, "options.chat.visibility.full"),
//      SYSTEM(1, "options.chat.visibility.system"),
//      HIDDEN(2, "options.chat.visibility.hidden");
//
//      private static final IntFunction<ChatVisiblity> BY_ID =
//          ByIdMap.continuous(v -> v.id, values(), ByIdMap.OutOfBoundsStrategy.WRAP);
//      public static final Codec<ChatVisiblity> LEGACY_CODEC =
//          Codec.INT.xmap(BY_ID::apply, v -> v.id);
//      private final int id;
//      private final Component caption;
//
//      ChatVisiblity(final int id, final String key) {
//         this.id = id;
//         this.caption = Component.translatable(key);
//      }
//      public Component caption() { return this.caption; }
//   }
//
// NOTE: the decompiled enum exposes NO getId()/getKey() methods — `id` is private
// and read only inside the BY_ID/LEGACY_CODEC lambdas, and the constructor's `key`
// string is consumed into a translatable Component (Component.translatable(key)).
// This header ports the bit-exact, registry-free observable data:
//   * ordinal()/name() (standard Java enum)
//   * the private `id` field value per constant
//   * the translation key string literal passed to the constructor
//   * ByIdMap.continuous(WRAP) decode: BY_ID.apply(int) == values[Math.floorMod(id,3)]
//     which is exactly the LEGACY_CODEC decode path.
// caption() returns a Component (network/component-coupled) and is intentionally NOT
// modelled here — only the underlying translation key constant is.

#include <cstdint>
#include <string_view>

namespace mc::world::entity::player {

// Ordinals follow declaration order, exactly as Java enum ordinal().
enum class ChatVisiblity : int32_t {
    FULL = 0,
    SYSTEM = 1,
    HIDDEN = 2,
};

// Number of enum constants (ChatVisiblity.values().length).
inline constexpr int32_t CHAT_VISIBILITY_COUNT = 3;

// Java enum constant identifier, i.e. name() — the UPPERCASE declared name.
inline constexpr std::string_view chatVisibilityName(ChatVisiblity v) {
    switch (v) {
        case ChatVisiblity::FULL:   return "FULL";
        case ChatVisiblity::SYSTEM: return "SYSTEM";
        case ChatVisiblity::HIDDEN: return "HIDDEN";
    }
    return {};
}

// The private `id` field value passed to the constructor (FULL=0, SYSTEM=1, HIDDEN=2).
// For ChatVisiblity these happen to equal ordinal(), but they are declared explicitly
// in the source, so port them as a distinct constant.
inline constexpr int32_t chatVisibilityId(ChatVisiblity v) {
    switch (v) {
        case ChatVisiblity::FULL:   return 0;
        case ChatVisiblity::SYSTEM: return 1;
        case ChatVisiblity::HIDDEN: return 2;
    }
    return -1;
}

// The translation-key string literal passed to the constructor (becomes the
// TranslatableContents key of caption()).
inline constexpr std::string_view chatVisibilityKey(ChatVisiblity v) {
    switch (v) {
        case ChatVisiblity::FULL:   return "options.chat.visibility.full";
        case ChatVisiblity::SYSTEM: return "options.chat.visibility.system";
        case ChatVisiblity::HIDDEN: return "options.chat.visibility.hidden";
    }
    return {};
}

// ByIdMap.continuous(idGetter, values(), WRAP) decode, i.e. BY_ID.apply(id) and the
// LEGACY_CODEC decode. continuous() builds a sorted-by-id array (here identical to the
// declaration order since ids are 0,1,2 contiguous), then WRAP yields
//   sortedValues[Mth.positiveModulo(id, length)]  where positiveModulo == Math.floorMod.
// Math.floorMod result has the sign of the divisor (always positive here, divisor=3),
// so the index is always in [0, 3).
inline constexpr ChatVisiblity chatVisibilityById(int32_t id) {
    // Math.floorMod(id, 3): r = id % 3; if (r != 0 && (r ^ 3) < 0) r += 3;
    int32_t r = id % 3;
    if (r != 0 && ((r ^ 3) < 0)) r += 3;
    // sortedValues[r]; ids 0/1/2 map to FULL/SYSTEM/HIDDEN in declaration order.
    switch (r) {
        case 0:  return ChatVisiblity::FULL;
        case 1:  return ChatVisiblity::SYSTEM;
        default: return ChatVisiblity::HIDDEN; // r == 2
    }
}

} // namespace mc::world::entity::player
