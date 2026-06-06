#include "SurfaceRuleData.h"
#include "Noises.h"
#include "../block/Blocks.h"

// Port of net.minecraft.data.worldgen.SurfaceRuleData.
// Each function mirrors its Java counterpart, referencing biome keys as
// "minecraft:<snake_case>" strings. Biome conditions are evaluated through the
// Java-shaped BiomeManager/BiomeSource pipeline in overworld surface generation.

namespace mc::levelgen::SurfaceRuleData {

namespace SR = SurfaceRules;

// Helper — create a blockState rule for a named block
static SR::RuleSourcePtr rule(const char* name) {
    uint32_t id = getDefaultBlockStateId(name, 0);
    return SR::blockState(id);
}

// Helper — noiseCondition with a single lower bound (max = +inf)
static SR::ConditionSourcePtr surfaceNoiseAbove(double threshold) {
    // Java: SurfaceRules.noiseCondition(Noises.SURFACE, threshold / 8.25, Double.MAX_VALUE)
    return SR::noiseCondition(Noises::SURFACE, threshold / 8.25);
}

SR::RuleSourcePtr overworld() {
    return overworldLike(true, false, true);
}

SR::RuleSourcePtr overworldLike(bool doPreliminarySurfaceCheck,
                                  bool bedrockRoof,
                                  bool bedrockFloor) {
    // Pre-built rule sources matching Java static fields
    const SR::RuleSourcePtr AIR              = rule("air");
    const SR::RuleSourcePtr BEDROCK          = rule("bedrock");
    const SR::RuleSourcePtr WHITE_TERRACOTTA = rule("white_terracotta");
    const SR::RuleSourcePtr ORANGE_TERRACOTTA= rule("orange_terracotta");
    const SR::RuleSourcePtr TERRACOTTA       = rule("terracotta");
    const SR::RuleSourcePtr RED_SAND         = rule("red_sand");
    const SR::RuleSourcePtr RED_SANDSTONE    = rule("red_sandstone");
    const SR::RuleSourcePtr STONE            = rule("stone");
    const SR::RuleSourcePtr DEEPSLATE        = rule("deepslate");
    const SR::RuleSourcePtr DIRT             = rule("dirt");
    const SR::RuleSourcePtr PODZOL           = rule("podzol");
    const SR::RuleSourcePtr COARSE_DIRT      = rule("coarse_dirt");
    const SR::RuleSourcePtr MYCELIUM         = rule("mycelium");
    const SR::RuleSourcePtr GRASS_BLOCK      = rule("grass_block");
    const SR::RuleSourcePtr CALCITE          = rule("calcite");
    const SR::RuleSourcePtr GRAVEL           = rule("gravel");
    const SR::RuleSourcePtr SAND             = rule("sand");
    const SR::RuleSourcePtr SANDSTONE        = rule("sandstone");
    const SR::RuleSourcePtr PACKED_ICE       = rule("packed_ice");
    const SR::RuleSourcePtr SNOW_BLOCK       = rule("snow_block");
    const SR::RuleSourcePtr MUD              = rule("mud");
    const SR::RuleSourcePtr POWDER_SNOW      = rule("powder_snow");
    const SR::RuleSourcePtr ICE              = rule("ice");
    const SR::RuleSourcePtr WATER            = rule("water");

    // ---- Named conditions ----
    auto woodedBadlandsTop        = SR::yBlockCheck(VerticalAnchor::absolute(97),  2);
    auto badlandsTop              = SR::yBlockCheck(VerticalAnchor::absolute(256), 0);
    auto badlandsHeightCondition  = SR::yStartCheck(VerticalAnchor::absolute(63), -1);
    auto badlandsMid              = SR::yStartCheck(VerticalAnchor::absolute(74),  1);
    auto mangroveSwampPuddleLevel = SR::yBlockCheck(VerticalAnchor::absolute(60),  0);
    auto swampPuddleLevel         = SR::yBlockCheck(VerticalAnchor::absolute(62),  0);
    auto aboveOverworldSeaLevel   = SR::yBlockCheck(VerticalAnchor::absolute(63),  0);
    auto notUnderwater            = SR::waterBlockCheck(-1,  0);
    auto aboveWater               = SR::waterBlockCheck( 0,  0);
    auto notUnderDeepWater        = SR::waterStartCheck(-6, -1);
    auto hole                     = SR::holeCond();
    auto steep                    = SR::steepCond();
    auto frozenOcean              = SR::isBiome({"minecraft:frozen_ocean", "minecraft:deep_frozen_ocean"});

    // grass_block if above water, else dirt
    auto grassOrDirtIfUnderwater = SR::sequence({
        SR::ifTrue(aboveWater, GRASS_BLOCK),
        DIRT
    });
    // sand if not on ceiling, sandstone on ceiling
    auto sandOrSandstoneIfCeiling = SR::sequence({
        SR::ifTrue(SR::ON_CEILING(), SANDSTONE),
        SAND
    });
    // gravel if not on ceiling, stone on ceiling
    auto gravelOrStoneIfCeiling = SR::sequence({
        SR::ifTrue(SR::ON_CEILING(), STONE),
        GRAVEL
    });

    auto biomesWithSandAndSandstone       = SR::isBiome({"minecraft:warm_ocean", "minecraft:beach", "minecraft:snowy_beach"});
    auto biomesWithSandAndVeryDeepSandstone = SR::isBiome({"minecraft:desert"});

    // Common surface and under-surface rules shared between biome groups
    auto commonSurfaceAndUnderRules = SR::sequence({
        SR::ifTrue(SR::isBiome({"minecraft:stony_peaks"}),
            SR::sequence({
                SR::ifTrue(SR::noiseCondition(Noises::CALCITE, -0.0125, 0.0125), CALCITE),
                STONE
            })),
        SR::ifTrue(SR::isBiome({"minecraft:stony_shore"}),
            SR::sequence({
                SR::ifTrue(SR::noiseCondition(Noises::GRAVEL, -0.05, 0.05), gravelOrStoneIfCeiling),
                STONE
            })),
        SR::ifTrue(SR::isBiome({"minecraft:windswept_hills"}),
            SR::ifTrue(surfaceNoiseAbove(1.0), STONE)),
        SR::ifTrue(biomesWithSandAndSandstone, sandOrSandstoneIfCeiling),
        SR::ifTrue(biomesWithSandAndVeryDeepSandstone, sandOrSandstoneIfCeiling),
        SR::ifTrue(SR::isBiome({"minecraft:dripstone_caves"}), STONE)
    });

    auto powderSnowUnderRule = SR::ifTrue(
        SR::noiseCondition(Noises::POWDER_SNOW, 0.45, 0.58),
        SR::ifTrue(aboveWater, POWDER_SNOW));

    auto powderSnowSurfaceRule = SR::ifTrue(
        SR::noiseCondition(Noises::POWDER_SNOW, 0.35, 0.6),
        SR::ifTrue(aboveWater, POWDER_SNOW));

    // Under-surface rule per biome
    auto biomeUnderSurfaceRule = SR::sequence({
        SR::ifTrue(SR::isBiome({"minecraft:frozen_peaks"}),
            SR::sequence({
                SR::ifTrue(steep, PACKED_ICE),
                SR::ifTrue(SR::noiseCondition(Noises::PACKED_ICE, -0.5, 0.2), PACKED_ICE),
                SR::ifTrue(SR::noiseCondition(Noises::ICE, -0.0625, 0.025), ICE),
                SR::ifTrue(aboveWater, SNOW_BLOCK)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:snowy_slopes"}),
            SR::sequence({
                SR::ifTrue(steep, STONE),
                powderSnowUnderRule,
                SR::ifTrue(aboveWater, SNOW_BLOCK)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:jagged_peaks"}), STONE),
        SR::ifTrue(SR::isBiome({"minecraft:grove"}),
            SR::sequence({ powderSnowUnderRule, DIRT })),
        commonSurfaceAndUnderRules,
        SR::ifTrue(SR::isBiome({"minecraft:windswept_savanna"}),
            SR::ifTrue(surfaceNoiseAbove(1.75), STONE)),
        SR::ifTrue(SR::isBiome({"minecraft:windswept_gravelly_hills"}),
            SR::sequence({
                SR::ifTrue(surfaceNoiseAbove(2.0),  gravelOrStoneIfCeiling),
                SR::ifTrue(surfaceNoiseAbove(1.0),  STONE),
                SR::ifTrue(surfaceNoiseAbove(-1.0), DIRT),
                gravelOrStoneIfCeiling
            })),
        SR::ifTrue(SR::isBiome({"minecraft:mangrove_swamp"}), MUD),
        DIRT
    });

    // Surface rule per biome
    auto biomeSurfaceRule = SR::sequence({
        SR::ifTrue(SR::isBiome({"minecraft:frozen_peaks"}),
            SR::sequence({
                SR::ifTrue(steep, PACKED_ICE),
                SR::ifTrue(SR::noiseCondition(Noises::PACKED_ICE, 0.0, 0.2), PACKED_ICE),
                SR::ifTrue(SR::noiseCondition(Noises::ICE, 0.0, 0.025), ICE),
                SR::ifTrue(aboveWater, SNOW_BLOCK)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:snowy_slopes"}),
            SR::sequence({
                SR::ifTrue(steep, STONE),
                powderSnowSurfaceRule,
                SR::ifTrue(aboveWater, SNOW_BLOCK)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:jagged_peaks"}),
            SR::sequence({
                SR::ifTrue(steep, STONE),
                SR::ifTrue(aboveWater, SNOW_BLOCK)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:grove"}),
            SR::sequence({ powderSnowSurfaceRule, SR::ifTrue(aboveWater, SNOW_BLOCK) })),
        commonSurfaceAndUnderRules,
        SR::ifTrue(SR::isBiome({"minecraft:windswept_savanna"}),
            SR::sequence({
                SR::ifTrue(surfaceNoiseAbove(1.75), STONE),
                SR::ifTrue(surfaceNoiseAbove(-0.5), COARSE_DIRT)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:windswept_gravelly_hills"}),
            SR::sequence({
                SR::ifTrue(surfaceNoiseAbove(2.0),  gravelOrStoneIfCeiling),
                SR::ifTrue(surfaceNoiseAbove(1.0),  STONE),
                SR::ifTrue(surfaceNoiseAbove(-1.0), grassOrDirtIfUnderwater),
                gravelOrStoneIfCeiling
            })),
        SR::ifTrue(SR::isBiome({"minecraft:old_growth_pine_taiga", "minecraft:old_growth_spruce_taiga"}),
            SR::sequence({
                SR::ifTrue(surfaceNoiseAbove(1.75), COARSE_DIRT),
                SR::ifTrue(surfaceNoiseAbove(-0.95), PODZOL)
            })),
        SR::ifTrue(SR::isBiome({"minecraft:ice_spikes"}),
            SR::ifTrue(aboveWater, SNOW_BLOCK)),
        SR::ifTrue(SR::isBiome({"minecraft:mangrove_swamp"}), MUD),
        SR::ifTrue(SR::isBiome({"minecraft:mushroom_fields"}), MYCELIUM),
        grassOrDirtIfUnderwater
    });

    // Surface noise thresholds for clay/band patterns (badlands)
    auto clayBand1 = SR::noiseCondition(Noises::SURFACE, -0.909, -0.5454);
    auto clayBand2 = SR::noiseCondition(Noises::SURFACE, -0.1818, 0.1818);
    auto clayBand3 = SR::noiseCondition(Noises::SURFACE,  0.5454, 0.909);

    // Main rule applied near the surface
    auto mainRuleCloseToSurface = SR::sequence({
        SR::ifTrue(SR::ON_FLOOR(),
            SR::sequence({
                // Wooded badlands topsoil
                SR::ifTrue(SR::isBiome({"minecraft:wooded_badlands"}),
                    SR::ifTrue(woodedBadlandsTop,
                        SR::sequence({
                            SR::ifTrue(clayBand1, COARSE_DIRT),
                            SR::ifTrue(clayBand2, COARSE_DIRT),
                            SR::ifTrue(clayBand3, COARSE_DIRT),
                            grassOrDirtIfUnderwater
                        }))),
                // Swamp water puddles
                SR::ifTrue(SR::isBiome({"minecraft:swamp"}),
                    SR::ifTrue(swampPuddleLevel,
                        SR::ifTrue(SR::notCond(aboveOverworldSeaLevel),
                            SR::ifTrue(SR::noiseCondition(Noises::SWAMP, 0.0), WATER)))),
                // Mangrove swamp water puddles
                SR::ifTrue(SR::isBiome({"minecraft:mangrove_swamp"}),
                    SR::ifTrue(mangroveSwampPuddleLevel,
                        SR::ifTrue(SR::notCond(aboveOverworldSeaLevel),
                            SR::ifTrue(SR::noiseCondition(Noises::SWAMP, 0.0), WATER))))
            })),

        // Badlands biomes
        SR::ifTrue(SR::isBiome({"minecraft:badlands", "minecraft:eroded_badlands", "minecraft:wooded_badlands"}),
            SR::sequence({
                SR::ifTrue(SR::ON_FLOOR(),
                    SR::sequence({
                        SR::ifTrue(badlandsTop,  ORANGE_TERRACOTTA),
                        SR::ifTrue(badlandsMid,
                            SR::sequence({
                                SR::ifTrue(clayBand1, TERRACOTTA),
                                SR::ifTrue(clayBand2, TERRACOTTA),
                                SR::ifTrue(clayBand3, TERRACOTTA),
                                SR::bandlands()
                            })),
                        SR::ifTrue(notUnderwater,
                            SR::sequence({
                                SR::ifTrue(SR::ON_CEILING(), RED_SANDSTONE),
                                RED_SAND
                            })),
                        SR::ifTrue(SR::notCond(hole), ORANGE_TERRACOTTA),
                        SR::ifTrue(notUnderDeepWater, WHITE_TERRACOTTA),
                        gravelOrStoneIfCeiling
                    })),
                SR::ifTrue(badlandsHeightCondition,
                    SR::sequence({
                        SR::ifTrue(aboveOverworldSeaLevel,
                            SR::ifTrue(SR::notCond(badlandsMid), ORANGE_TERRACOTTA)),
                        SR::bandlands()
                    })),
                SR::ifTrue(SR::UNDER_FLOOR(),
                    SR::ifTrue(notUnderDeepWater, WHITE_TERRACOTTA))
            })),

        // Standard floor — biome surface
        SR::ifTrue(SR::ON_FLOOR(),
            SR::ifTrue(notUnderwater,
                SR::sequence({
                    SR::ifTrue(frozenOcean,
                        SR::ifTrue(hole,
                            SR::sequence({
                                SR::ifTrue(aboveWater, AIR),
                                SR::ifTrue(SR::temperatureCond(), ICE),
                                WATER
                            }))),
                    biomeSurfaceRule
                }))),

        // Under-floor rules
        SR::ifTrue(notUnderDeepWater,
            SR::sequence({
                SR::ifTrue(SR::ON_FLOOR(),
                    SR::ifTrue(frozenOcean, SR::ifTrue(hole, WATER))),
                SR::ifTrue(SR::UNDER_FLOOR(), biomeUnderSurfaceRule),
                SR::ifTrue(biomesWithSandAndSandstone,
                    SR::ifTrue(SR::DEEP_UNDER_FLOOR(), SANDSTONE)),
                SR::ifTrue(biomesWithSandAndVeryDeepSandstone,
                    SR::ifTrue(SR::VERY_DEEP_UNDER_FLOOR(), SANDSTONE))
            })),

        // Catch-all floor (peaks, ocean floors)
        SR::ifTrue(SR::ON_FLOOR(),
            SR::sequence({
                SR::ifTrue(SR::isBiome({"minecraft:frozen_peaks", "minecraft:jagged_peaks"}), STONE),
                SR::ifTrue(SR::isBiome({"minecraft:warm_ocean", "minecraft:lukewarm_ocean",
                                         "minecraft:deep_lukewarm_ocean"}),
                    sandOrSandstoneIfCeiling),
                gravelOrStoneIfCeiling
            }))
    });

    std::vector<SR::RuleSourcePtr> parts;

    if (bedrockRoof) {
        parts.push_back(
            SR::ifTrue(
                SR::notCond(SR::verticalGradient("bedrock_roof",
                    VerticalAnchor::belowTop(5), VerticalAnchor::top())),
                BEDROCK));
    }

    if (bedrockFloor) {
        parts.push_back(
            SR::ifTrue(
                SR::verticalGradient("bedrock_floor",
                    VerticalAnchor::bottom(), VerticalAnchor::aboveBottom(5)),
                BEDROCK));
    }

    if (doPreliminarySurfaceCheck) {
        parts.push_back(SR::ifTrue(SR::abovePreliminarySurfaceCond(), mainRuleCloseToSurface));
    } else {
        parts.push_back(mainRuleCloseToSurface);
    }

    // Deepslate gradient: Y 0..8
    parts.push_back(
        SR::ifTrue(
            SR::verticalGradient("deepslate",
                VerticalAnchor::absolute(0), VerticalAnchor::absolute(8)),
            DEEPSLATE));

    return SR::sequence(std::move(parts));
}

SR::RuleSourcePtr nether() {
    const SR::RuleSourcePtr BEDROCK        = rule("bedrock");
    const SR::RuleSourcePtr NETHERRACK     = rule("netherrack");
    const SR::RuleSourcePtr SOUL_SAND      = rule("soul_sand");
    const SR::RuleSourcePtr SOUL_SOIL      = rule("soul_soil");
    const SR::RuleSourcePtr BASALT         = rule("basalt");
    const SR::RuleSourcePtr BLACKSTONE     = rule("blackstone");
    const SR::RuleSourcePtr GRAVEL         = rule("gravel");
    const SR::RuleSourcePtr LAVA           = rule("lava");
    const SR::RuleSourcePtr WARPED_WART_BLK= rule("warped_wart_block");
    const SR::RuleSourcePtr WARPED_NYLIUM  = rule("warped_nylium");
    const SR::RuleSourcePtr NETHER_WART_BLK= rule("nether_wart_block");
    const SR::RuleSourcePtr CRIMSON_NYLIUM = rule("crimson_nylium");

    auto aboveNetherLavaLevel       = SR::yBlockCheck(VerticalAnchor::absolute(31), 0);
    auto aboveNetherLavaSurface     = SR::yBlockCheck(VerticalAnchor::absolute(32), 0);
    auto netherBandAroundBottom     = SR::yStartCheck(VerticalAnchor::absolute(30), 0);
    auto netherBandAroundTop        = SR::notCond(SR::yStartCheck(VerticalAnchor::absolute(35), 0));
    auto closeToCeiling             = SR::yBlockCheck(VerticalAnchor::belowTop(5), 0);
    auto hole                       = SR::holeCond();
    auto soulSandLayer              = SR::noiseCondition(Noises::SOUL_SAND_LAYER,  -0.012);
    auto gravelLayer                = SR::noiseCondition(Noises::GRAVEL_LAYER,     -0.012);
    auto patch                      = SR::noiseCondition(Noises::PATCH,             -0.012);
    auto netherrack                 = SR::noiseCondition(Noises::NETHERRACK,         0.54);
    auto netherWart                 = SR::noiseCondition(Noises::NETHER_WART,        1.17);
    auto netherStateSelector        = SR::noiseCondition(Noises::NETHER_STATE_SELECTOR, 0.0);

    auto gravelPatch = SR::ifTrue(patch,
        SR::ifTrue(netherBandAroundBottom, SR::ifTrue(netherBandAroundTop, GRAVEL)));

    return SR::sequence({
        SR::ifTrue(SR::verticalGradient("bedrock_floor",
                    VerticalAnchor::bottom(), VerticalAnchor::aboveBottom(5)), BEDROCK),
        SR::ifTrue(SR::notCond(SR::verticalGradient("bedrock_roof",
                    VerticalAnchor::belowTop(5), VerticalAnchor::top())), BEDROCK),
        SR::ifTrue(closeToCeiling, NETHERRACK),
        SR::ifTrue(SR::isBiome({"minecraft:basalt_deltas"}),
            SR::sequence({
                SR::ifTrue(SR::UNDER_CEILING(), BASALT),
                SR::ifTrue(SR::UNDER_FLOOR(),
                    SR::sequence({
                        gravelPatch,
                        SR::ifTrue(netherStateSelector, BASALT),
                        BLACKSTONE
                    }))
            })),
        SR::ifTrue(SR::isBiome({"minecraft:soul_sand_valley"}),
            SR::sequence({
                SR::ifTrue(SR::UNDER_CEILING(),
                    SR::sequence({ SR::ifTrue(netherStateSelector, SOUL_SAND), SOUL_SOIL })),
                SR::ifTrue(SR::UNDER_FLOOR(),
                    SR::sequence({
                        gravelPatch,
                        SR::ifTrue(netherStateSelector, SOUL_SAND),
                        SOUL_SOIL
                    }))
            })),
        SR::ifTrue(SR::ON_FLOOR(),
            SR::sequence({
                SR::ifTrue(SR::notCond(aboveNetherLavaSurface), SR::ifTrue(hole, LAVA)),
                SR::ifTrue(SR::isBiome({"minecraft:warped_forest"}),
                    SR::ifTrue(SR::notCond(netherrack),
                        SR::ifTrue(aboveNetherLavaLevel,
                            SR::sequence({
                                SR::ifTrue(netherWart, WARPED_WART_BLK),
                                WARPED_NYLIUM
                            })))),
                SR::ifTrue(SR::isBiome({"minecraft:crimson_forest"}),
                    SR::ifTrue(SR::notCond(netherrack),
                        SR::ifTrue(aboveNetherLavaLevel,
                            SR::sequence({
                                SR::ifTrue(netherWart, NETHER_WART_BLK),
                                CRIMSON_NYLIUM
                            }))))
            })),
        SR::ifTrue(SR::isBiome({"minecraft:nether_wastes"}),
            SR::sequence({
                SR::ifTrue(SR::UNDER_FLOOR(),
                    SR::ifTrue(soulSandLayer,
                        SR::sequence({
                            SR::ifTrue(SR::notCond(hole),
                                SR::ifTrue(netherBandAroundBottom,
                                    SR::ifTrue(netherBandAroundTop, SOUL_SAND))),
                            NETHERRACK
                        }))),
                SR::ifTrue(SR::ON_FLOOR(),
                    SR::ifTrue(aboveNetherLavaLevel,
                        SR::ifTrue(netherBandAroundTop,
                            SR::ifTrue(gravelLayer,
                                SR::sequence({
                                    SR::ifTrue(aboveNetherLavaSurface, GRAVEL),
                                    SR::ifTrue(SR::notCond(hole), GRAVEL)
                                })))))
            })),
        NETHERRACK
    });
}

SR::RuleSourcePtr end() {
    return rule("end_stone");
}

} // namespace mc::levelgen::SurfaceRuleData
