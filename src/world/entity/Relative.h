// 1:1 C++ port of net.minecraft.world.entity.Relative (Minecraft 26.1.2).
//
// NOTE ON NAMING: the assignment refers to this class as "RelativeMovement"
// (its name in older Minecraft versions). In 26.1.2 the decompiled source names
// it net.minecraft.world.entity.Relative; the semantics — a Set<Relative> packed
// to/from an int bitfield used by player teleport/movement packets — are
// identical. We mirror the 26.1.2 source verbatim.
//
// Java source (26.1.2/src/net/minecraft/world/entity/Relative.java):
//
//   public enum Relative {
//      X(0), Y(1), Z(2), Y_ROT(3), X_ROT(4),
//      DELTA_X(5), DELTA_Y(6), DELTA_Z(7), ROTATE_DELTA(8);
//      public static final Set<Relative> ALL = Set.of(values());
//      public static final Set<Relative> ROTATION = Set.of(X_ROT, Y_ROT);
//      public static final Set<Relative> DELTA = Set.of(DELTA_X, DELTA_Y, DELTA_Z, ROTATE_DELTA);
//      ...
//      private final int bit;
//      Relative(final int bit) { this.bit = bit; }
//      private int getMask()            { return 1 << this.bit; }
//      private boolean isSet(int value) { return (value & getMask()) == getMask(); }
//      public static Set<Relative> unpack(int value) {
//         Set<Relative> r = EnumSet.noneOf(Relative.class);
//         for (Relative a : values()) if (a.isSet(value)) r.add(a);
//         return r;
//      }
//      public static int pack(Set<Relative> set) {
//         int r = 0; for (Relative a : set) r |= a.getMask(); return r;
//      }
//   }
//
// This is a pure bitset enum — no registries/world/components. We model the
// Set<Relative> as a 9-bit mask (one bit per constant, at the constant's `bit`
// position), exactly matching pack()'s output, and provide pack/unpack helpers
// plus the static ALL/ROTATION/DELTA sets and the rotation/position/direction/
// union constructors. The parity gate (RelativeParityTest) checks these against
// the REAL net.minecraft.world.entity.Relative.
//
// SKIPPED (network-coupled, not portable as a pure header):
//   SET_STREAM_CODEC = ByteBufCodecs.INT.map(Relative::unpack, Relative::pack)
// — it is a thin StreamCodec wrapper over pack/unpack (the int it reads/writes is
// exactly pack()'s output), so porting pack/unpack covers the wire value.

#ifndef MCPP_WORLD_ENTITY_RELATIVE_H
#define MCPP_WORLD_ENTITY_RELATIVE_H

#include <array>
#include <cstdint>
#include <string_view>

namespace mc {

// The nine Relative constants, in Java declaration order (= ordinal()).
// The enum's underlying value IS the Java `bit` field (which here equals the
// ordinal for every constant, 0..8 — but we key everything off `bit` exactly as
// the Java does via getMask()).
enum class Relative : int {
    X = 0,
    Y = 1,
    Z = 2,
    Y_ROT = 3,
    X_ROT = 4,
    DELTA_X = 5,
    DELTA_Y = 6,
    DELTA_Z = 7,
    ROTATE_DELTA = 8,
};

// Java enum's implicit values() array, in declaration order.
inline constexpr std::array<Relative, 9> RELATIVE_VALUES = {
    Relative::X,       Relative::Y,       Relative::Z,
    Relative::Y_ROT,   Relative::X_ROT,   Relative::DELTA_X,
    Relative::DELTA_Y, Relative::DELTA_Z, Relative::ROTATE_DELTA,
};

// Relative.ordinal() — position in declaration order.
inline constexpr int ordinal(Relative r) { return static_cast<int>(r); }

// The Java `bit` field. In 26.1.2 it equals the ordinal for every constant
// (X(0)..ROTATE_DELTA(8)), but we keep this distinct to track the source exactly.
inline constexpr int bit(Relative r) {
    switch (r) {
        case Relative::X:            return 0;
        case Relative::Y:            return 1;
        case Relative::Z:            return 2;
        case Relative::Y_ROT:        return 3;
        case Relative::X_ROT:        return 4;
        case Relative::DELTA_X:      return 5;
        case Relative::DELTA_Y:      return 6;
        case Relative::DELTA_Z:      return 7;
        case Relative::ROTATE_DELTA: return 8;
    }
    return 0; // unreachable
}

// Relative.name() — verbatim Java constant name.
inline constexpr std::string_view name(Relative r) {
    switch (r) {
        case Relative::X:            return "X";
        case Relative::Y:            return "Y";
        case Relative::Z:            return "Z";
        case Relative::Y_ROT:        return "Y_ROT";
        case Relative::X_ROT:        return "X_ROT";
        case Relative::DELTA_X:      return "DELTA_X";
        case Relative::DELTA_Y:      return "DELTA_Y";
        case Relative::DELTA_Z:      return "DELTA_Z";
        case Relative::ROTATE_DELTA: return "ROTATE_DELTA";
    }
    return ""; // unreachable
}

// private int getMask() { return 1 << this.bit; }
inline constexpr int getMask(Relative r) { return 1 << bit(r); }

// private boolean isSet(int value) { return (value & getMask()) == getMask(); }
inline constexpr bool isSet(Relative r, int value) {
    return (value & getMask(r)) == getMask(r);
}

// A Set<Relative> represented as the packed int bitfield (one bit per constant
// at its `bit` position). This is precisely what pack() yields and what unpack()
// consumes, so a RelativeSet IS the canonical packed representation.
using RelativeSet = std::int32_t;

// Set membership test against the packed-int representation.
inline constexpr bool contains(RelativeSet set, Relative r) {
    return isSet(r, static_cast<int>(set));
}

// public static int pack(Set<Relative> set):
//   int result = 0; for (Relative a : set) result |= a.getMask(); return result;
// Here the "set" is already the packed representation, so pack(unpack(v)) == v
// for any v whose only set bits are among the nine masks. We expose pack() over
// an iterable of Relative constants for the constructor helpers below.
template <typename It>
inline constexpr int pack(It begin, It end) {
    int result = 0;
    for (It it = begin; it != end; ++it) result |= getMask(*it);
    return result;
}

// public static Set<Relative> unpack(int value):
//   for (Relative a : values()) if (a.isSet(value)) result.add(a);
// Returns the packed RelativeSet containing exactly the constants whose bit is
// set in `value`. (Bits in `value` outside the nine masks are dropped, exactly
// as unpack() never adds a constant for them.)
inline constexpr RelativeSet unpack(int value) {
    int result = 0;
    for (Relative a : RELATIVE_VALUES) {
        if (isSet(a, value)) result |= getMask(a);
    }
    return static_cast<RelativeSet>(result);
}

// pack(Set<Relative>) over the canonical RelativeSet: identity on the nine valid
// bits (a packed set IS the int), so we re-pack via unpack to drop any stray bits
// — matching pack(unpack(v)).
inline constexpr int pack(RelativeSet set) {
    int result = 0;
    for (Relative a : RELATIVE_VALUES) {
        if (contains(set, a)) result |= getMask(a);
    }
    return result;
}

// public static final Set<Relative> ALL = Set.of(values());
inline constexpr RelativeSet RELATIVE_ALL =
    getMask(Relative::X) | getMask(Relative::Y) | getMask(Relative::Z) |
    getMask(Relative::Y_ROT) | getMask(Relative::X_ROT) |
    getMask(Relative::DELTA_X) | getMask(Relative::DELTA_Y) |
    getMask(Relative::DELTA_Z) | getMask(Relative::ROTATE_DELTA);

// public static final Set<Relative> ROTATION = Set.of(X_ROT, Y_ROT);
inline constexpr RelativeSet RELATIVE_ROTATION =
    getMask(Relative::X_ROT) | getMask(Relative::Y_ROT);

// public static final Set<Relative> DELTA = Set.of(DELTA_X, DELTA_Y, DELTA_Z, ROTATE_DELTA);
inline constexpr RelativeSet RELATIVE_DELTA =
    getMask(Relative::DELTA_X) | getMask(Relative::DELTA_Y) |
    getMask(Relative::DELTA_Z) | getMask(Relative::ROTATE_DELTA);

// public static Set<Relative> rotation(boolean relativeYRot, boolean relativeXRot):
//   add Y_ROT if relativeYRot; add X_ROT if relativeXRot.
inline constexpr RelativeSet rotation(bool relativeYRot, bool relativeXRot) {
    int relatives = 0;
    if (relativeYRot) relatives |= getMask(Relative::Y_ROT);
    if (relativeXRot) relatives |= getMask(Relative::X_ROT);
    return static_cast<RelativeSet>(relatives);
}

// public static Set<Relative> position(boolean relativeX, boolean relativeY, boolean relativeZ):
//   add X / Y / Z respectively.
inline constexpr RelativeSet position(bool relativeX, bool relativeY, bool relativeZ) {
    int relatives = 0;
    if (relativeX) relatives |= getMask(Relative::X);
    if (relativeY) relatives |= getMask(Relative::Y);
    if (relativeZ) relatives |= getMask(Relative::Z);
    return static_cast<RelativeSet>(relatives);
}

// public static Set<Relative> direction(boolean relativeX, boolean relativeY, boolean relativeZ):
//   add DELTA_X / DELTA_Y / DELTA_Z respectively.
inline constexpr RelativeSet direction(bool relativeX, bool relativeY, bool relativeZ) {
    int relatives = 0;
    if (relativeX) relatives |= getMask(Relative::DELTA_X);
    if (relativeY) relatives |= getMask(Relative::DELTA_Y);
    if (relativeZ) relatives |= getMask(Relative::DELTA_Z);
    return static_cast<RelativeSet>(relatives);
}

// @SafeVarargs public static Set<Relative> union(Set<Relative>... sets):
//   HashSet; addAll each. As packed ints this is a bitwise OR. We expose the
//   binary form (the only thing the packed value can express).
inline constexpr RelativeSet unionSets(RelativeSet a, RelativeSet b) {
    return static_cast<RelativeSet>(static_cast<int>(a) | static_cast<int>(b));
}

} // namespace mc

#endif // MCPP_WORLD_ENTITY_RELATIVE_H
