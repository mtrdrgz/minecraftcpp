#pragma once

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/phys/shapes/BooleanOp.java (26.1.2) — the 16
// two-input boolean tables used by Shapes.join/joinIsNotEmpty.
//
// Naming: TRUE/FALSE collide with Windows macros, so those two constants are
// ALWAYS_TRUE / ALWAYS_FALSE; everything else keeps the Java name.
// ---------------------------------------------------------------------------

namespace mc {

class BooleanOp {
public:
    using Fn = bool (*)(bool, bool);
    constexpr explicit BooleanOp(Fn fn) noexcept : fn_(fn) {}
    // Java: BooleanOp.apply(first, second) — BooleanOp.java:21.
    constexpr bool apply(bool first, bool second) const noexcept { return fn_(first, second); }

private:
    Fn fn_;
};

namespace BooleanOps {
// BooleanOp.java:4-19, in declaration order.
inline constexpr BooleanOp ALWAYS_FALSE{+[](bool, bool) { return false; }};            // FALSE
inline constexpr BooleanOp NOT_OR{+[](bool f, bool s) { return !f && !s; }};
inline constexpr BooleanOp ONLY_SECOND{+[](bool f, bool s) { return s && !f; }};
inline constexpr BooleanOp NOT_FIRST{+[](bool f, bool) { return !f; }};
inline constexpr BooleanOp ONLY_FIRST{+[](bool f, bool s) { return f && !s; }};
inline constexpr BooleanOp NOT_SECOND{+[](bool, bool s) { return !s; }};
inline constexpr BooleanOp NOT_SAME{+[](bool f, bool s) { return f != s; }};
inline constexpr BooleanOp NOT_AND{+[](bool f, bool s) { return !f || !s; }};
inline constexpr BooleanOp AND{+[](bool f, bool s) { return f && s; }};
inline constexpr BooleanOp SAME{+[](bool f, bool s) { return f == s; }};
inline constexpr BooleanOp SECOND{+[](bool, bool s) { return s; }};
inline constexpr BooleanOp CAUSES{+[](bool f, bool s) { return !f || s; }};
inline constexpr BooleanOp FIRST{+[](bool f, bool) { return f; }};
inline constexpr BooleanOp CAUSED_BY{+[](bool f, bool s) { return f || !s; }};
inline constexpr BooleanOp OR{+[](bool f, bool s) { return f || s; }};
inline constexpr BooleanOp ALWAYS_TRUE{+[](bool, bool) { return true; }};              // TRUE
} // namespace BooleanOps

} // namespace mc
