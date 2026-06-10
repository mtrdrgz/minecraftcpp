// 1:1 port of the volume()/pitch() float pair for every public-static SoundType
// constant in net.minecraft.world.level.block.SoundType (Minecraft 26.1.2).
//
// SOURCE: 26.1.2/src/net/minecraft/world/level/block/SoundType.java
//
// Each constant is `new SoundType(volume, pitch, break, step, place, hit, fall)`.
// The five SoundEvent fields are registry/asset-coupled (Holder<SoundEvent>) and
// are INTENTIONALLY NOT ported here — see SoundTypeParityTest.cpp / the assignment
// brief. Only the two plain `public final float` fields (volume, pitch), exposed
// via `public float getVolume()` / `public float getPitch()` (lines 846-852 of the
// Java), are reproduced. They are copied VERBATIM from the constructor literals.
//
// In the Java, every constant passes volume=1.0F, pitch=1.0F EXCEPT:
//   METAL          : volume 1.0F, pitch 1.5F   (line 31-33)
//   ANVIL          : volume 0.3F, pitch 1.0F   (line 58-60)
//   TWISTING_VINES : volume 1.0F, pitch 0.5F   (line 160-168)
//
// The order below is the source declaration order (EMPTY first), which is also the
// order the GT tool emits when it reflects over SoundType's public static fields.

#pragma once

#include <cstddef>

namespace mc::block {

struct SoundTypeData {
    const char* name;  // the Java field name (e.g. "GRASS")
    float volume;      // getVolume()
    float pitch;       // getPitch()
};

// Declaration order matches SoundType.java top-to-bottom.
inline constexpr SoundTypeData SOUND_TYPES[] = {
    {"EMPTY", 1.0F, 1.0F},
    {"WOOD", 1.0F, 1.0F},
    {"GRAVEL", 1.0F, 1.0F},
    {"GRASS", 1.0F, 1.0F},
    {"LILY_PAD", 1.0F, 1.0F},
    {"STONE", 1.0F, 1.0F},
    {"METAL", 1.0F, 1.5F},
    {"GLASS", 1.0F, 1.0F},
    {"WOOL", 1.0F, 1.0F},
    {"SAND", 1.0F, 1.0F},
    {"SNOW", 1.0F, 1.0F},
    {"POWDER_SNOW", 1.0F, 1.0F},
    {"LADDER", 1.0F, 1.0F},
    {"ANVIL", 0.3F, 1.0F},
    {"SLIME_BLOCK", 1.0F, 1.0F},
    {"HONEY_BLOCK", 1.0F, 1.0F},
    {"WET_GRASS", 1.0F, 1.0F},
    {"CORAL_BLOCK", 1.0F, 1.0F},
    {"BAMBOO", 1.0F, 1.0F},
    {"BAMBOO_SAPLING", 1.0F, 1.0F},
    {"SCAFFOLDING", 1.0F, 1.0F},
    {"SWEET_BERRY_BUSH", 1.0F, 1.0F},
    {"CROP", 1.0F, 1.0F},
    {"HARD_CROP", 1.0F, 1.0F},
    {"VINE", 1.0F, 1.0F},
    {"NETHER_WART", 1.0F, 1.0F},
    {"LANTERN", 1.0F, 1.0F},
    {"STEM", 1.0F, 1.0F},
    {"NYLIUM", 1.0F, 1.0F},
    {"FUNGUS", 1.0F, 1.0F},
    {"ROOTS", 1.0F, 1.0F},
    {"SHROOMLIGHT", 1.0F, 1.0F},
    {"WEEPING_VINES", 1.0F, 1.0F},
    {"TWISTING_VINES", 1.0F, 0.5F},
    {"SOUL_SAND", 1.0F, 1.0F},
    {"SOUL_SOIL", 1.0F, 1.0F},
    {"BASALT", 1.0F, 1.0F},
    {"WART_BLOCK", 1.0F, 1.0F},
    {"NETHERRACK", 1.0F, 1.0F},
    {"NETHER_BRICKS", 1.0F, 1.0F},
    {"NETHER_SPROUTS", 1.0F, 1.0F},
    {"NETHER_ORE", 1.0F, 1.0F},
    {"BONE_BLOCK", 1.0F, 1.0F},
    {"NETHERITE_BLOCK", 1.0F, 1.0F},
    {"ANCIENT_DEBRIS", 1.0F, 1.0F},
    {"LODESTONE", 1.0F, 1.0F},
    {"CHAIN", 1.0F, 1.0F},
    {"NETHER_GOLD_ORE", 1.0F, 1.0F},
    {"GILDED_BLACKSTONE", 1.0F, 1.0F},
    {"CANDLE", 1.0F, 1.0F},
    {"AMETHYST", 1.0F, 1.0F},
    {"AMETHYST_CLUSTER", 1.0F, 1.0F},
    {"SMALL_AMETHYST_BUD", 1.0F, 1.0F},
    {"MEDIUM_AMETHYST_BUD", 1.0F, 1.0F},
    {"LARGE_AMETHYST_BUD", 1.0F, 1.0F},
    {"TUFF", 1.0F, 1.0F},
    {"TUFF_BRICKS", 1.0F, 1.0F},
    {"POLISHED_TUFF", 1.0F, 1.0F},
    {"CALCITE", 1.0F, 1.0F},
    {"DRIPSTONE_BLOCK", 1.0F, 1.0F},
    {"POINTED_DRIPSTONE", 1.0F, 1.0F},
    {"COPPER", 1.0F, 1.0F},
    {"COPPER_BULB", 1.0F, 1.0F},
    {"COPPER_GRATE", 1.0F, 1.0F},
    {"COPPER_GOLEM_STATUE", 1.0F, 1.0F},
    {"CAVE_VINES", 1.0F, 1.0F},
    {"SPORE_BLOSSOM", 1.0F, 1.0F},
    {"CACTUS_FLOWER", 1.0F, 1.0F},
    {"AZALEA", 1.0F, 1.0F},
    {"FLOWERING_AZALEA", 1.0F, 1.0F},
    {"MOSS_CARPET", 1.0F, 1.0F},
    {"PINK_PETALS", 1.0F, 1.0F},
    {"LEAF_LITTER", 1.0F, 1.0F},
    {"MOSS", 1.0F, 1.0F},
    {"BIG_DRIPLEAF", 1.0F, 1.0F},
    {"SMALL_DRIPLEAF", 1.0F, 1.0F},
    {"ROOTED_DIRT", 1.0F, 1.0F},
    {"HANGING_ROOTS", 1.0F, 1.0F},
    {"AZALEA_LEAVES", 1.0F, 1.0F},
    {"SCULK_SENSOR", 1.0F, 1.0F},
    {"SCULK_CATALYST", 1.0F, 1.0F},
    {"SCULK", 1.0F, 1.0F},
    {"SCULK_VEIN", 1.0F, 1.0F},
    {"SCULK_SHRIEKER", 1.0F, 1.0F},
    {"GLOW_LICHEN", 1.0F, 1.0F},
    {"DEEPSLATE", 1.0F, 1.0F},
    {"DEEPSLATE_BRICKS", 1.0F, 1.0F},
    {"DEEPSLATE_TILES", 1.0F, 1.0F},
    {"POLISHED_DEEPSLATE", 1.0F, 1.0F},
    {"FROGLIGHT", 1.0F, 1.0F},
    {"FROGSPAWN", 1.0F, 1.0F},
    {"MANGROVE_ROOTS", 1.0F, 1.0F},
    {"MUDDY_MANGROVE_ROOTS", 1.0F, 1.0F},
    {"MUD", 1.0F, 1.0F},
    {"MUD_BRICKS", 1.0F, 1.0F},
    {"PACKED_MUD", 1.0F, 1.0F},
    {"HANGING_SIGN", 1.0F, 1.0F},
    {"NETHER_WOOD_HANGING_SIGN", 1.0F, 1.0F},
    {"BAMBOO_WOOD_HANGING_SIGN", 1.0F, 1.0F},
    {"BAMBOO_WOOD", 1.0F, 1.0F},
    {"NETHER_WOOD", 1.0F, 1.0F},
    {"CHERRY_WOOD", 1.0F, 1.0F},
    {"CHERRY_SAPLING", 1.0F, 1.0F},
    {"CHERRY_LEAVES", 1.0F, 1.0F},
    {"CHERRY_WOOD_HANGING_SIGN", 1.0F, 1.0F},
    {"CHISELED_BOOKSHELF", 1.0F, 1.0F},
    {"SHELF", 1.0F, 1.0F},
    {"SUSPICIOUS_SAND", 1.0F, 1.0F},
    {"SUSPICIOUS_GRAVEL", 1.0F, 1.0F},
    {"DECORATED_POT", 1.0F, 1.0F},
    {"DECORATED_POT_CRACKED", 1.0F, 1.0F},
    {"TRIAL_SPAWNER", 1.0F, 1.0F},
    {"SPONGE", 1.0F, 1.0F},
    {"WET_SPONGE", 1.0F, 1.0F},
    {"VAULT", 1.0F, 1.0F},
    {"CREAKING_HEART", 1.0F, 1.0F},
    {"HEAVY_CORE", 1.0F, 1.0F},
    {"COBWEB", 1.0F, 1.0F},
    {"SPAWNER", 1.0F, 1.0F},
    {"RESIN", 1.0F, 1.0F},
    {"RESIN_BRICKS", 1.0F, 1.0F},
    {"IRON", 1.0F, 1.0F},
    {"DRIED_GHAST", 1.0F, 1.0F},
};

inline constexpr std::size_t SOUND_TYPE_COUNT =
    sizeof(SOUND_TYPES) / sizeof(SOUND_TYPES[0]);

}  // namespace mc::block
