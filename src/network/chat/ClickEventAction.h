#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of net/minecraft/network/chat/ClickEvent.java — the Action enum only
// (Minecraft Java Edition 26.1.2).
//
// ClickEvent.Action is a StringRepresentable enum with EXACTLY EIGHT constants,
// in this declaration order (ClickEvent.java:25-32):
//   OPEN_URL("open_url", true)            ordinal 0
//   OPEN_FILE("open_file", false)         ordinal 1
//   RUN_COMMAND("run_command", true)      ordinal 2
//   SUGGEST_COMMAND("suggest_command",true) ordinal 3
//   SHOW_DIALOG("show_dialog", true)      ordinal 4
//   CHANGE_PAGE("change_page", true)      ordinal 5
//   COPY_TO_CLIPBOARD("copy_to_clipboard",true) ordinal 6
//   CUSTOM("custom", true)                ordinal 7
//
// Ported here, verbatim from the Java ctor args (ClickEvent.java:40-44) and
// accessors:
//   getSerializedName()      -> the `name` field        (ClickEvent.java:50-53)
//   isAllowedFromServer()    -> the `allowFromServer`    (ClickEvent.java:46-48)
//
// NOT ported (out of scope / coupled to the codec machinery):
//   - UNSAFE_CODEC / CODEC (StringRepresentable.fromEnum + validate)  ClickEvent.java:34-35
//   - valueCodec()           -> per-constant MapCodec                 ClickEvent.java:55-57
//   - filterForSerialization -> DataResult error/success wrapper      ClickEvent.java:59-63
//   These need DataFixerUpper codecs and the per-action record CODECs; they are
//   hard-absent here, not faked.
// ---------------------------------------------------------------------------

namespace mc {

// Java: ClickEvent.Action enum constants, in declaration order (ordinals 0..7).
// (ClickEvent.java:25-32)
enum class ClickEventAction : int32_t {
    OPEN_URL = 0,
    OPEN_FILE = 1,
    RUN_COMMAND = 2,
    SUGGEST_COMMAND = 3,
    SHOW_DIALOG = 4,
    CHANGE_PAGE = 5,
    COPY_TO_CLIPBOARD = 6,
    CUSTOM = 7,
};

inline constexpr int CLICK_EVENT_ACTION_COUNT = 8;

// Per-constant serialized name — the `name` ctor arg (ClickEvent.java:25-32),
// returned by getSerializedName() (ClickEvent.java:50-53). Indexed by ordinal.
inline constexpr const char* CLICK_EVENT_ACTION_NAME[CLICK_EVENT_ACTION_COUNT] = {
    "open_url",          // OPEN_URL
    "open_file",         // OPEN_FILE
    "run_command",       // RUN_COMMAND
    "suggest_command",   // SUGGEST_COMMAND
    "show_dialog",       // SHOW_DIALOG
    "change_page",       // CHANGE_PAGE
    "copy_to_clipboard", // COPY_TO_CLIPBOARD
    "custom",            // CUSTOM
};

// Per-constant allowFromServer flag — the `allowFromServer` ctor arg
// (ClickEvent.java:25-32), returned by isAllowedFromServer()
// (ClickEvent.java:46-48). Indexed by ordinal.
//
// NOTE: OPEN_FILE is the only action NOT allowed from the server (false at
// ClickEvent.java:26); every other action is true.
inline constexpr bool CLICK_EVENT_ACTION_ALLOW_FROM_SERVER[CLICK_EVENT_ACTION_COUNT] = {
    true,  // OPEN_URL
    false, // OPEN_FILE
    true,  // RUN_COMMAND
    true,  // SUGGEST_COMMAND
    true,  // SHOW_DIALOG
    true,  // CHANGE_PAGE
    true,  // COPY_TO_CLIPBOARD
    true,  // CUSTOM
};

// Java: ClickEvent.Action.getSerializedName() — ClickEvent.java:50-53.
constexpr const char* clickEventActionSerializedName(ClickEventAction v) noexcept {
    return CLICK_EVENT_ACTION_NAME[static_cast<int>(v)];
}

// Java: ClickEvent.Action.isAllowedFromServer() — ClickEvent.java:46-48.
constexpr bool clickEventActionIsAllowedFromServer(ClickEventAction v) noexcept {
    return CLICK_EVENT_ACTION_ALLOW_FROM_SERVER[static_cast<int>(v)];
}

} // namespace mc
