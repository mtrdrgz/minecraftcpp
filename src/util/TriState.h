// 1:1 C++ port of net.minecraft.util.TriState (26.1.2).
//
// Java source (26.1.2/src/net/minecraft/util/TriState.java):
//   public enum TriState implements StringRepresentable {
//      TRUE("true"),
//      FALSE("false"),
//      DEFAULT("default");
//
//      public static final Codec<TriState> CODEC = ...;   // (NOT PORTED — see below)
//      private final String name;
//
//      TriState(final String name) { this.name = name; }
//
//      public static TriState from(final boolean value) {
//         return value ? TRUE : FALSE;
//      }
//
//      public boolean toBoolean(final boolean defaultValue) {
//         return switch (this) {
//            case TRUE -> true;
//            case FALSE -> false;
//            default -> defaultValue;        // DEFAULT
//         };
//      }
//
//      @Override
//      public String getSerializedName() { return this.name; }
//   }
//
// This is a pure value enum: no floats, no RNG, no ordering subtleties. Every
// branch maps directly onto the Java switch. The enum ordinal order is
// TRUE=0, FALSE=1, DEFAULT=2 (declaration order) — preserved so values() and
// getSerializedName() match.
//
// NOT PORTED (serialization-coupled, out of scope for a pure value gate):
//   - public static final Codec<TriState> CODEC
//     (Codec.either(Codec.BOOL, StringRepresentable.fromEnum(...)).xmap(...)).
//   The getSerializedName() string used by that codec IS ported and gated.
//
// NOTE: this Minecraft 26.1.2 TriState has NO `toBooleanOrElse(Supplier)` /
// `toBooleanOrElse(boolean)` method — the only boolean helper is
// `toBoolean(boolean)`. (Verified by grep across 26.1.2/src.)
#ifndef MCPP_UTIL_TRISTATE_H
#define MCPP_UTIL_TRISTATE_H

#include <string>

namespace mc::util {

// Declaration order matches the Java enum (ordinal): TRUE=0, FALSE=1, DEFAULT=2.
enum class TriState : int {
    TRUE = 0,
    FALSE = 1,
    DEFAULT = 2,
};

// public static TriState from(final boolean value) { return value ? TRUE : FALSE; }
inline TriState triStateFrom(bool value) {
    return value ? TriState::TRUE : TriState::FALSE;
}

// public boolean toBoolean(final boolean defaultValue)
//   switch (this) { case TRUE -> true; case FALSE -> false; default -> defaultValue; }
inline bool triStateToBoolean(TriState self, bool defaultValue) {
    switch (self) {
        case TriState::TRUE:
            return true;
        case TriState::FALSE:
            return false;
        default:  // DEFAULT
            return defaultValue;
    }
}

// @Override public String getSerializedName() { return this.name; }
//   TRUE("true"), FALSE("false"), DEFAULT("default")
inline std::string triStateGetSerializedName(TriState self) {
    switch (self) {
        case TriState::TRUE:
            return "true";
        case TriState::FALSE:
            return "false";
        default:  // DEFAULT
            return "default";
    }
}

}  // namespace mc::util

#endif  // MCPP_UTIL_TRISTATE_H
