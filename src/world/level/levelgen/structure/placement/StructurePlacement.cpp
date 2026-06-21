#include "StructurePlacement.h"

#include "../../RandomSource.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace mc::levelgen::structure {

namespace {

// java.lang.Math.floorDiv(int, int)
int floorDiv(int x, int y) {
    int q = x / y;
    if ((x ^ y) < 0 && q * y != x) {
        --q;
    }
    return q;
}

WorldgenRandom newRandom() {
    // Java: new WorldgenRandom(new LegacyRandomSource(0L))
    return WorldgenRandom(std::make_shared<LegacyRandomSource>(0));
}

// RandomSpreadType.evaluate
int evaluateSpread(RandomSpreadType type, WorldgenRandom& random, int limit) {
    switch (type) {
        case RandomSpreadType::LINEAR:
            return random.nextInt(limit);
        case RandomSpreadType::TRIANGULAR:
            return (random.nextInt(limit) + random.nextInt(limit)) / 2;
    }
    return 0;
}

// --- StructurePlacement.FrequencyReducer implementations (verbatim) ---

// probabilityReducer: random.setLargeFeatureWithSalt(seed, salt, sourceX, sourceZ)
//   note the Java argument order: (seed, x=salt, z=sourceX, blend=sourceZ)
bool probabilityReducer(int64_t seed, int salt, int sourceX, int sourceZ, float probability) {
    WorldgenRandom random = newRandom();
    random.setLargeFeatureWithSalt(seed, salt, sourceX, sourceZ);
    return random.nextFloat() < probability;
}

// legacyProbabilityReducerWithDouble: setLargeFeatureSeed(seed, sourceX, sourceZ); nextDouble()
bool legacyProbabilityReducerWithDouble(int64_t seed, int /*salt*/, int sourceX, int sourceZ, float probability) {
    WorldgenRandom random = newRandom();
    random.setLargeFeatureSeed(seed, sourceX, sourceZ);
    return random.nextDouble() < static_cast<double>(probability);
}

// legacyArbitrarySaltProbabilityReducer: setLargeFeatureWithSalt(seed, sourceX, sourceZ, 10387320); nextFloat()
bool legacyArbitrarySaltProbabilityReducer(int64_t seed, int /*salt*/, int sourceX, int sourceZ, float probability) {
    WorldgenRandom random = newRandom();
    random.setLargeFeatureWithSalt(seed, sourceX, sourceZ, 10387320);
    return random.nextFloat() < probability;
}

// legacyPillagerOutpostReducer
bool legacyPillagerOutpostReducer(int64_t seed, int /*salt*/, int sourceX, int sourceZ, float probability) {
    int cx = sourceX >> 4;
    int cz = sourceZ >> 4;
    WorldgenRandom random = newRandom();
    // Java: random.setSeed((long)(cx ^ cz << 4) ^ seed) — '<<' binds before '^';
    // (cx ^ (cz << 4)) is computed as int, sign-extended, then XORed with seed.
    int32_t mixed = cx ^ (cz << 4);
    random.setSeed(static_cast<int64_t>(mixed) ^ seed);
    random.nextInt();
    return random.nextInt(static_cast<int>(1.0F / probability)) == 0;
}

bool runReducer(FrequencyReductionMethod m, int64_t seed, int salt, int sourceX, int sourceZ, float probability) {
    switch (m) {
        case FrequencyReductionMethod::DEFAULT:       return probabilityReducer(seed, salt, sourceX, sourceZ, probability);
        case FrequencyReductionMethod::LEGACY_TYPE_1: return legacyPillagerOutpostReducer(seed, salt, sourceX, sourceZ, probability);
        case FrequencyReductionMethod::LEGACY_TYPE_2: return legacyArbitrarySaltProbabilityReducer(seed, salt, sourceX, sourceZ, probability);
        case FrequencyReductionMethod::LEGACY_TYPE_3: return legacyProbabilityReducerWithDouble(seed, salt, sourceX, sourceZ, probability);
    }
    return true;
}

FrequencyReductionMethod parseFrm(const std::string& s) {
    if (s == "legacy_type_1") return FrequencyReductionMethod::LEGACY_TYPE_1;
    if (s == "legacy_type_2") return FrequencyReductionMethod::LEGACY_TYPE_2;
    if (s == "legacy_type_3") return FrequencyReductionMethod::LEGACY_TYPE_3;
    return FrequencyReductionMethod::DEFAULT; // "default"
}

bool isKnownBrokenRuntimeStructureSet(const std::string& /*id*/) {
    // Villages were gated off here while the jigsaw polish layer was incomplete.
    // That layer is now ported: legacy_single_pool_element air-ignore + the
    // RuleProcessor family (street/farm/mossify) + the TERRAIN_MATCHING
    // GravityProcessor (streets follow terrain) + the Beardifier (beard_thin terrain
    // adaptation, certified byte-exact). Villages are enabled.
    return false;
}

} // namespace

StructureState StructureState::loadFromDirectory(const std::string& structureSetDir, int64_t levelSeed) {
    StructureState state;
    state.levelSeed = levelSeed;

    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(structureSetDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

        std::ifstream in(entry.path());
        if (!in) continue;
        nlohmann::json j;
        in >> j;
        const auto& pj = j.at("placement");

        StructurePlacement p;
        const std::string typeStr = pj.at("type").get<std::string>();
        if (typeStr == "minecraft:concentric_rings") {
            p.type = PlacementType::CONCENTRIC_RINGS;
        } else {
            p.type = PlacementType::RANDOM_SPREAD;
        }

        p.salt = pj.at("salt").get<int>();
        if (pj.contains("frequency")) p.frequency = pj.at("frequency").get<float>();
        if (pj.contains("frequency_reduction_method"))
            p.frequencyReductionMethod = parseFrm(pj.at("frequency_reduction_method").get<std::string>());
        if (pj.contains("locate_offset")) {
            const auto& off = pj.at("locate_offset");
            p.locateOffsetX = off.at(0).get<int>();
            p.locateOffsetY = off.at(1).get<int>();
            p.locateOffsetZ = off.at(2).get<int>();
        }
        if (pj.contains("exclusion_zone")) {
            const auto& ez = pj.at("exclusion_zone");
            ExclusionZone z;
            z.otherSet = ez.at("other_set").get<std::string>();
            z.chunkCount = ez.at("chunk_count").get<int>();
            p.exclusionZone = z;
        }

        if (p.type == PlacementType::RANDOM_SPREAD) {
            p.spacing = pj.at("spacing").get<int>();
            p.separation = pj.at("separation").get<int>();
            if (pj.contains("spread_type") && pj.at("spread_type").get<std::string>() == "triangular")
                p.spreadType = RandomSpreadType::TRIANGULAR;
            else
                p.spreadType = RandomSpreadType::LINEAR;
        }

        // Key by "minecraft:<filename-without-ext>".
        const std::string id = "minecraft:" + entry.path().stem().string();
        if (isKnownBrokenRuntimeStructureSet(id)) {
            p.generationEnabled = false;
        }
        state.sets[id] = p;
    }
    return state;
}

void StructureState::getPotentialStructureChunk(const StructurePlacement& p, int sourceX, int sourceZ,
                                                int& outX, int& outZ) const {
    int spacedGridX = floorDiv(sourceX, p.spacing);
    int spacedGridZ = floorDiv(sourceZ, p.spacing);
    WorldgenRandom random = newRandom();
    random.setLargeFeatureWithSalt(levelSeed, spacedGridX, spacedGridZ, p.salt);
    int limit = p.spacing - p.separation;
    int spreadX = evaluateSpread(p.spreadType, random, limit);
    int spreadZ = evaluateSpread(p.spreadType, random, limit);
    outX = spacedGridX * p.spacing + spreadX;
    outZ = spacedGridZ * p.spacing + spreadZ;
}

bool StructureState::hasStructureChunkInRange(const std::string& otherSet, int sourceX, int sourceZ, int range) const {
    auto it = sets.find(otherSet);
    if (it == sets.end()) return false;
    const StructurePlacement& placement = it->second;
    for (int testX = sourceX - range; testX <= sourceX + range; ++testX) {
        for (int testZ = sourceZ - range; testZ <= sourceZ + range; ++testZ) {
            if (isStructureChunk(placement, testX, testZ)) return true;
        }
    }
    return false;
}

bool StructureState::isStructureChunk(const StructurePlacement& p, int sourceX, int sourceZ) const {
    // isPlacementChunk (random_spread only; concentric_rings is unported)
    if (p.type != PlacementType::RANDOM_SPREAD) return false;
    int px, pz;
    getPotentialStructureChunk(p, sourceX, sourceZ, px, pz);
    if (px != sourceX || pz != sourceZ) return false;

    // applyAdditionalChunkRestrictions (frequency reduction)
    if (p.frequency < 1.0F &&
        !runReducer(p.frequencyReductionMethod, levelSeed, p.salt, sourceX, sourceZ, p.frequency)) {
        return false;
    }

    // applyInteractionsWithOtherStructures (exclusion zone)
    if (p.exclusionZone.has_value()) {
        const ExclusionZone& z = *p.exclusionZone;
        if (hasStructureChunkInRange(z.otherSet, sourceX, sourceZ, z.chunkCount)) return false;
    }
    return true;
}

} // namespace mc::levelgen::structure
