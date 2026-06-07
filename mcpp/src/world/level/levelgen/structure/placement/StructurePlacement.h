#pragma once

// Port of net.minecraft.world.level.levelgen.structure.placement.StructurePlacement
// and RandomSpreadStructurePlacement. This decides, for a given world seed, which
// chunk coordinates are "structure chunks" for each structure set — the first half
// of "structures generate 1:1" (where they can spawn). Piece/jigsaw assembly is a
// separate stage.
//
// Everything here is a direct translation of the decompiled 26.1.2 Java; the
// values (spacing/separation/salt/frequency/spread_type/exclusion) come from
// 26.1.2/data/minecraft/worldgen/structure_set/*.json.

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace mc::levelgen::structure {

// net.minecraft...RandomSpreadType
enum class RandomSpreadType { LINEAR, TRIANGULAR };

// net.minecraft...StructurePlacement.FrequencyReductionMethod
enum class FrequencyReductionMethod {
    DEFAULT,        // "default"        -> probabilityReducer
    LEGACY_TYPE_1,  // "legacy_type_1"  -> legacyPillagerOutpostReducer
    LEGACY_TYPE_2,  // "legacy_type_2"  -> legacyArbitrarySaltProbabilityReducer
    LEGACY_TYPE_3,  // "legacy_type_3"  -> legacyProbabilityReducerWithDouble
};

enum class PlacementType { RANDOM_SPREAD, CONCENTRIC_RINGS };

struct ExclusionZone {
    std::string otherSet;   // e.g. "minecraft:villages"
    int chunkCount = 0;
};

// One structure set's placement (the "placement" object of a structure_set JSON).
struct StructurePlacement {
    PlacementType type = PlacementType::RANDOM_SPREAD;

    // StructurePlacement base fields.
    int locateOffsetX = 0, locateOffsetY = 0, locateOffsetZ = 0;
    FrequencyReductionMethod frequencyReductionMethod = FrequencyReductionMethod::DEFAULT;
    float frequency = 1.0F;
    int salt = 0;
    std::optional<ExclusionZone> exclusionZone;

    // RandomSpreadStructurePlacement fields.
    int spacing = 1;
    int separation = 0;
    RandomSpreadType spreadType = RandomSpreadType::LINEAR;
};

// Holds every loaded structure set, keyed by id ("minecraft:villages"). This is
// the C++ stand-in for the relevant slice of ChunkGeneratorStructureState: it owns
// the level seed and lets exclusion zones look up other sets by id.
class StructureState {
public:
    int64_t levelSeed = 0;
    std::map<std::string, StructurePlacement> sets;

    // Loads every *.json under data/minecraft/worldgen/structure_set into `sets`.
    static StructureState loadFromDirectory(const std::string& structureSetDir, int64_t levelSeed);

    // RandomSpreadStructurePlacement.getPotentialStructureChunk
    void getPotentialStructureChunk(const StructurePlacement& p, int sourceX, int sourceZ,
                                    int& outX, int& outZ) const;

    // StructurePlacement.isStructureChunk (placement + frequency + exclusion)
    bool isStructureChunk(const StructurePlacement& p, int sourceX, int sourceZ) const;

    // ChunkGeneratorStructureState.hasStructureChunkInRange
    bool hasStructureChunkInRange(const std::string& otherSet, int sourceX, int sourceZ, int range) const;

    // True for placement types this port does not yet certify (concentric_rings).
    static bool isSupported(const StructurePlacement& p) { return p.type == PlacementType::RANDOM_SPREAD; }
};

} // namespace mc::levelgen::structure
