#pragma once

// Port of net.minecraft.data.worldgen.SurfaceRuleData

#include "SurfaceRules.h"

namespace mc::levelgen::SurfaceRuleData {

// Returns the surface rule for the standard overworld (with preliminary-surface
// check, no bedrock roof, bedrock floor enabled).
SurfaceRules::RuleSourcePtr overworld();

// Returns the overworld-like rule with configurable options.
SurfaceRules::RuleSourcePtr overworldLike(bool doPreliminarySurfaceCheck,
                                           bool bedrockRoof,
                                           bool bedrockFloor);

// Returns the surface rule for the Nether.
SurfaceRules::RuleSourcePtr nether();

// Returns the surface rule for the End (just end stone everywhere).
SurfaceRules::RuleSourcePtr end();

} // namespace mc::levelgen::SurfaceRuleData
