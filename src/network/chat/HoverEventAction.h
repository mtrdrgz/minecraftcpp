#pragma once
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Port of net.minecraft.network.chat.HoverEvent.Action (Minecraft Java Edition
// 26.1.2 — 26.1.2/src/net/minecraft/network/chat/HoverEvent.java:24-58).
//
// A StringRepresentable enum with three constants, declared in this order
// (HoverEvent.java:25-27):
//   SHOW_TEXT  ("show_text",  true, ShowText.CODEC)
//   SHOW_ITEM  ("show_item",  true, ShowItem.CODEC)
//   SHOW_ENTITY("show_entity",true, ShowEntity.CODEC)
//
// Ported here VERBATIM from the Java (the bounded, dependency-free surface):
//   getSerializedName()    HoverEvent.java:45-48  — returns the `name` field
//   isAllowedFromServer()  HoverEvent.java:41-43  — returns the `allowFromServer` field
//   toString()             HoverEvent.java:50-53  — "<action " + name + ">"
//   ordinal() / name()     implicit JLS enum surface (declaration order / constant id)
//
// NOT ported (deliberately ABSENT, not faked):
//   the `codec` field (MapCodec<? extends HoverEvent>) and the dispatch CODEC /
//   UNSAFE_CODEC / filterForSerialization — these pull in DataFixerUpper Codec
//   machinery and the ShowText/ShowItem/ShowEntity record bodies (registries,
//   Component, ItemStackTemplate). The assignment scopes those OUT.
// ---------------------------------------------------------------------------

namespace mc::network::chat {

// Java: HoverEvent.Action enum constants, in declaration order (ordinals 0..2).
// (HoverEvent.java:25-27)
enum class HoverEventAction : int32_t {
    SHOW_TEXT = 0,
    SHOW_ITEM = 1,
    SHOW_ENTITY = 2,
};

inline constexpr int HOVER_EVENT_ACTION_COUNT = 3;

// Java enum constant name() — the identifier, indexed by ordinal.
inline constexpr const char* HOVER_EVENT_ACTION_ENUM_NAME[HOVER_EVENT_ACTION_COUNT] = {
    "SHOW_TEXT",   // 0
    "SHOW_ITEM",   // 1
    "SHOW_ENTITY", // 2
};

// Per-constant serialized name — the 1st ctor arg (HoverEvent.java:25-27).
// Indexed by ordinal.
inline constexpr const char* HOVER_EVENT_ACTION_SERIALIZED_NAME[HOVER_EVENT_ACTION_COUNT] = {
    "show_text",   // SHOW_TEXT
    "show_item",   // SHOW_ITEM
    "show_entity", // SHOW_ENTITY
};

// Per-constant allowFromServer — the 2nd ctor arg (HoverEvent.java:25-27).
// Indexed by ordinal. All three are `true` in 26.1.2.
inline constexpr bool HOVER_EVENT_ACTION_ALLOW_FROM_SERVER[HOVER_EVENT_ACTION_COUNT] = {
    true,  // SHOW_TEXT
    true,  // SHOW_ITEM
    true,  // SHOW_ENTITY
};

// Java: HoverEvent.Action.name() — the enum constant identifier.
constexpr const char* hoverEventActionName(HoverEventAction v) noexcept {
    return HOVER_EVENT_ACTION_ENUM_NAME[static_cast<int>(v)];
}

// Java: HoverEvent.Action.ordinal().
constexpr int hoverEventActionOrdinal(HoverEventAction v) noexcept {
    return static_cast<int>(v);
}

// Java: HoverEvent.Action.getSerializedName() — HoverEvent.java:45-48.
constexpr const char* hoverEventActionSerializedName(HoverEventAction v) noexcept {
    return HOVER_EVENT_ACTION_SERIALIZED_NAME[static_cast<int>(v)];
}

// Java: HoverEvent.Action.isAllowedFromServer() — HoverEvent.java:41-43.
constexpr bool hoverEventActionIsAllowedFromServer(HoverEventAction v) noexcept {
    return HOVER_EVENT_ACTION_ALLOW_FROM_SERVER[static_cast<int>(v)];
}

// Java: HoverEvent.Action.toString() — "<action " + name + ">" (HoverEvent.java:50-53).
// Here `name` is the serialized name field (not the enum identifier).
inline std::string hoverEventActionToString(HoverEventAction v) {
    return std::string("<action ") + hoverEventActionSerializedName(v) + ">";
}

} // namespace mc::network::chat
