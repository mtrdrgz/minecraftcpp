#pragma once
// Port of net.minecraft.world.level.GameType (MC 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/level/GameType.java):
//   public enum GameType implements StringRepresentable {
//      SURVIVAL(0, "survival"), CREATIVE(1, "creative"),
//      ADVENTURE(2, "adventure"), SPECTATOR(3, "spectator");
//      ...
//      public int getId() { return this.id; }
//      public String getName() { return this.name; }
//      @Override public String getSerializedName() { return this.name; }
//      public boolean isBlockPlacingRestricted() { return this == ADVENTURE || this == SPECTATOR; }
//      public boolean isCreative() { return this == CREATIVE; }
//      public boolean isSurvival() { return this == SURVIVAL || this == ADVENTURE; }
//      public static GameType byId(final int id) { return BY_ID.apply(id); }
//      public static GameType byName(final String name) { return byName(name, SURVIVAL); }
//      public static @Nullable GameType byName(final String name, final @Nullable GameType defaultMode) {
//          GameType result = CODEC.byName(name);
//          return result != null ? result : defaultMode;
//      }
//      public static int getNullableId(final @Nullable GameType gameType) { return gameType != null ? gameType.id : -1; }
//      public static @Nullable GameType byNullableId(final int id) { return id == -1 ? null : byId(id); }
//      public static boolean isValidId(final int id) { return Arrays.stream(values()).anyMatch(gt -> gt.id == id); }
//   }
//
// byId uses ByIdMap.continuous(GameType::getId, values(), OutOfBoundsStrategy.ZERO)
//   (ByIdMap.java:66-78): with continuous ids 0..3, sortedValues[id] == the GameType
//   with that id; for out-of-range id it yields sortedValues[0] == SURVIVAL.
// byName resolves via StringRepresentable.EnumCodec.byName (StringRepresentable.java:79-81)
//   -> createNameLookup (StringRepresentable.java:46-61): for <=16 values, a linear scan
//   returning the constant whose getSerializedName().equals(name), else null.
//
// SCOPE: this header ports the PURE methods only (id/name/predicates/lookups).
// The Component shortName/longName display-name fields/methods (Component.translatable)
// and updatePlayerAbilities (mutates entity.player.Abilities) are NOT ported here —
// they depend on chat/component + entity subsystems. See unportedMethods.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mc::world::level {

// Ordinals follow declaration order, exactly as Java enum ordinal().
// The (id) constructor argument happens to equal the ordinal for every constant.
enum class GameType : int32_t {
    SURVIVAL = 0,
    CREATIVE = 1,
    ADVENTURE = 2,
    SPECTATOR = 3,
};

// GameType.values().length
inline constexpr int32_t GAME_TYPE_COUNT = 4;

// The constructor id argument (GameType.getId()). For these constants id == ordinal.
inline constexpr int32_t gameTypeGetId(GameType v) {
    switch (v) {
        case GameType::SURVIVAL:  return 0;
        case GameType::CREATIVE:  return 1;
        case GameType::ADVENTURE: return 2;
        case GameType::SPECTATOR: return 3;
    }
    return 0;
}

// Java enum constant identifier, i.e. name() — the UPPERCASE declared name.
inline constexpr std::string_view gameTypeEnumName(GameType v) {
    switch (v) {
        case GameType::SURVIVAL:  return "SURVIVAL";
        case GameType::CREATIVE:  return "CREATIVE";
        case GameType::ADVENTURE: return "ADVENTURE";
        case GameType::SPECTATOR: return "SPECTATOR";
    }
    return {};
}

// GameType.getName() == getSerializedName() — the lowercase serialized id (ctor arg).
inline constexpr std::string_view gameTypeGetName(GameType v) {
    switch (v) {
        case GameType::SURVIVAL:  return "survival";
        case GameType::CREATIVE:  return "creative";
        case GameType::ADVENTURE: return "adventure";
        case GameType::SPECTATOR: return "spectator";
    }
    return {};
}

// getSerializedName() returns the same string as getName().
inline constexpr std::string_view gameTypeGetSerializedName(GameType v) {
    return gameTypeGetName(v);
}

// isBlockPlacingRestricted() == (this == ADVENTURE || this == SPECTATOR)
inline constexpr bool gameTypeIsBlockPlacingRestricted(GameType v) {
    return v == GameType::ADVENTURE || v == GameType::SPECTATOR;
}

// isCreative() == (this == CREATIVE)
inline constexpr bool gameTypeIsCreative(GameType v) {
    return v == GameType::CREATIVE;
}

// isSurvival() == (this == SURVIVAL || this == ADVENTURE)
inline constexpr bool gameTypeIsSurvival(GameType v) {
    return v == GameType::SURVIVAL || v == GameType::ADVENTURE;
}

// byId(int): ByIdMap.continuous(..., ZERO). Ids are continuous 0..3, so an in-range
// id maps directly to that GameType; an out-of-range id yields sortedValues[0]==SURVIVAL.
inline constexpr GameType gameTypeById(int32_t id) {
    if (id >= 0 && id < GAME_TYPE_COUNT) {
        // sortedValues[id]: continuous, so the constant with getId()==id.
        switch (id) {
            case 0: return GameType::SURVIVAL;
            case 1: return GameType::CREATIVE;
            case 2: return GameType::ADVENTURE;
            case 3: return GameType::SPECTATOR;
        }
    }
    return GameType::SURVIVAL; // zeroValue == sortedValues[0]
}

// CODEC.byName(name): linear scan returning the constant whose serialized name equals
// `name`, or no value (Java null) if none matches.
inline std::optional<GameType> gameTypeCodecByName(std::string_view name) {
    for (int32_t i = 0; i < GAME_TYPE_COUNT; ++i) {
        auto v = static_cast<GameType>(i);
        if (gameTypeGetSerializedName(v) == name) return v;
    }
    return std::nullopt;
}

// byName(name, defaultMode): CODEC.byName(name) if present, else defaultMode.
inline GameType gameTypeByName(std::string_view name, GameType defaultMode) {
    auto r = gameTypeCodecByName(name);
    return r ? *r : defaultMode;
}

// byName(name): byName(name, SURVIVAL).
inline GameType gameTypeByName(std::string_view name) {
    return gameTypeByName(name, GameType::SURVIVAL);
}

// getNullableId(gameType): nullopt -> -1, else getId().
inline constexpr int32_t gameTypeGetNullableId(std::optional<GameType> v) {
    return v ? gameTypeGetId(*v) : -1;
}

// byNullableId(id): id == -1 -> nullopt, else byId(id).
inline constexpr std::optional<GameType> gameTypeByNullableId(int32_t id) {
    if (id == -1) return std::nullopt;
    return gameTypeById(id);
}

// isValidId(id): true iff some constant has getId()==id (ids are 0..3).
inline constexpr bool gameTypeIsValidId(int32_t id) {
    for (int32_t i = 0; i < GAME_TYPE_COUNT; ++i) {
        if (gameTypeGetId(static_cast<GameType>(i)) == id) return true;
    }
    return false;
}

// The player-ability flags GameType.updatePlayerAbilities mutates (net.minecraft.world.entity.
// player.Abilities). Only the booleans the mapping touches; flyingSpeed/walkingSpeed are untouched.
struct Abilities {
    bool mayfly = false;
    bool instabuild = false;
    bool invulnerable = false;
    bool flying = false;
    bool mayBuild = true;
};

// 1:1 port of GameType.updatePlayerAbilities (GameType.java:62-80): CREATIVE leaves `flying`
// UNCHANGED (only mayfly/instabuild/invulnerable set); SPECTATOR forces flying=true; SURVIVAL/
// ADVENTURE clear all four; mayBuild = !isBlockPlacingRestricted() always.
inline constexpr void gameTypeUpdatePlayerAbilities(GameType v, Abilities& a) {
    if (v == GameType::CREATIVE) {
        a.mayfly = true;
        a.instabuild = true;
        a.invulnerable = true;
    } else if (v == GameType::SPECTATOR) {
        a.mayfly = true;
        a.instabuild = false;
        a.invulnerable = true;
        a.flying = true;
    } else {
        a.mayfly = false;
        a.instabuild = false;
        a.invulnerable = false;
        a.flying = false;
    }
    a.mayBuild = !gameTypeIsBlockPlacingRestricted(v);
}

} // namespace mc::world::level
