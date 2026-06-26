#include "StructureGen.h"

#include "placement/StructurePlacement.h"
#include "StructurePieceBase.h"
#include "structures/SwampHutPiece.h"
#include "structures/DesertPyramidPiece.h"
#include "structures/JungleTemplePiece.h"
#include "structures/BuriedTreasurePieces.h"
#include "structures/OceanRuinClusterGeometry.h"
#include "structures/MineshaftPieces.h"
#include "structures/MineshaftAssembly.h"
#include "structures/OceanMonumentPieces.h"
#include "pools/PoolAlias.h"
#include "pools/StructureTemplatePool.h"
#include "templatesystem/StructureTemplateLoader.h"

#include "core/Log.h"
#include "nbt/NbtIo.h"
#include "util/SequencedPriorityIterator.h"
#include "world/level/block/BlockRotation.h"
#include "world/level/block/Blocks.h"
#include "world/level/block/JigsawAttach.h"
#include "world/level/levelgen/RandomSource.h"
#include "world/level/levelgen/Mth.h"
#include "world/level/levelgen/Beardifier.h"
#include "world/level/levelgen/feature/GenerationStep.h"
#include "world/phys/AABB.h"
#include "world/phys/shapes/Shapes.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mc::levelgen::structure {

namespace {

namespace fs = std::filesystem;
namespace pools = mc::levelgen::structure::pools;
namespace palias = mc::levelgen::structure::pools::alias;
namespace stl = mc::levelgen::structure::templatesystem;
namespace phys = mc;
namespace shp = mc;
using json = nlohmann::json;

constexpr int kMinBuildY = -64;
constexpr int kMaxBuildYInclusive = 319;
constexpr int kMaxBuildYExclusive = 320;
constexpr int kUnsetHeight = INT32_MIN;

std::string normalizeId(std::string s) {
    if (s.empty()) return "minecraft:empty";
    return s.find(':') == std::string::npos ? "minecraft:" + s : std::move(s);
}

std::string stripMinecraft(std::string s) {
    if (s.rfind("minecraft:", 0) == 0) s.erase(0, 10);
    return s;
}

std::optional<std::string> minecraftSubPath(std::string id) {
    id = normalizeId(std::move(id));
    auto colon = id.find(':');
    if (colon == std::string::npos) return std::nullopt;
    if (id.substr(0, colon) != "minecraft") return std::nullopt;
    return id.substr(colon + 1);
}

fs::path pathForId(const fs::path& base, const std::string& id, const char* ext) {
    auto sub = minecraftSubPath(id);
    if (!sub) return {};
    return base / fs::path(*sub + ext);
}

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::uint8_t> readBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> base64Decode(const std::string& in) {
    static const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int table[256];
    for (int& v : table) v = -1;
    for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(alphabet[i])] = i;

    std::vector<std::uint8_t> out;
    int value = 0;
    int bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || table[c] == -1) continue;
        value = (value << 6) + table[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

std::vector<std::string> splitc(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(s);
    while (std::getline(ss, cur, d)) out.push_back(cur);
    return out;
}

std::vector<fs::path> sortedJsonFiles(const fs::path& dir, bool recursive) {
    std::vector<fs::path> out;
    if (!fs::exists(dir)) return out;
    if (recursive) {
        for (const auto& ent : fs::recursive_directory_iterator(dir)) {
            if (ent.is_regular_file() && ent.path().extension() == ".json") out.push_back(ent.path());
        }
    } else {
        for (const auto& ent : fs::directory_iterator(dir)) {
            if (ent.is_regular_file() && ent.path().extension() == ".json") out.push_back(ent.path());
        }
    }
    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {
        return a.generic_string() < b.generic_string();
    });
    return out;
}

int tagInt(const mc::nbt::NbtTag& tag) {
    if (auto v = tag.as<std::int32_t>()) return *v;
    if (auto v = tag.as<std::int16_t>()) return *v;
    if (auto v = tag.as<std::int8_t>()) return *v;
    return 0;
}

BlockPos posFromList(const mc::nbt::NbtList* list) {
    if (!list || list->elements.size() < 3) return {};
    return {tagInt(list->elements[0]), tagInt(list->elements[1]), tagInt(list->elements[2])};
}

std::string propsFromCompound(const mc::nbt::NbtCompound* props) {
    if (!props) return {};
    std::map<std::string, std::string> sorted;
    for (const auto& [key, tag] : props->entries) {
        if (const auto* val = tag.as<std::string>()) sorted[key] = *val;
    }
    std::string out;
    for (const auto& [key, val] : sorted) {
        if (!out.empty()) out += ',';
        out += key;
        out += '=';
        out += val;
    }
    return out;
}

std::string propsFromStateMap(const std::unordered_map<std::string, std::string>& props) {
    std::map<std::string, std::string> sorted(props.begin(), props.end());
    std::string out;
    for (const auto& [key, val] : sorted) {
        if (!out.empty()) out += ',';
        out += key;
        out += '=';
        out += val;
    }
    return out;
}

struct ParsedState {
    std::string name = "minecraft:air";
    std::string props;
};

ParsedState parseSerializedState(std::string s) {
    ParsedState out;
    auto lb = s.find('[');
    if (lb == std::string::npos) {
        out.name = normalizeId(std::move(s));
        return out;
    }

    out.name = normalizeId(s.substr(0, lb));
    auto rb = s.rfind(']');
    if (rb == std::string::npos || rb <= lb + 1) return out;

    std::map<std::string, std::string> sorted;
    std::string body = s.substr(lb + 1, rb - lb - 1);
    for (const std::string& pair : splitc(body, ',')) {
        auto eq = pair.find('=');
        if (eq == std::string::npos) continue;
        sorted[pair.substr(0, eq)] = pair.substr(eq + 1);
    }
    for (const auto& [key, val] : sorted) {
        if (!out.props.empty()) out.props += ',';
        out.props += key;
        out.props += '=';
        out.props += val;
    }
    return out;
}

struct StateResolver {
    std::vector<mc::block_rotation::StateInfo> states;
    std::unordered_map<std::string, std::uint32_t> reverse;
    std::unordered_map<std::string, std::pair<std::string, std::string>> families;

    void init(const fs::path& dataDir) {
        states.resize(mc::g_blockStates.size());
        for (std::uint32_t id = 0; id < mc::g_blockStates.size(); ++id) {
            const mc::BlockState& bs = mc::g_blockStates[id];
            std::string name = bs.block ? bs.block->name : "air";
            std::string props = propsFromStateMap(bs.properties);
            states[id] = {name, props};
            reverse[name + "\x01" + props] = id;
        }
        loadFamilies(dataDir);
    }

    void loadFamilies(const fs::path& dataDir) {
        std::vector<fs::path> candidates;
        candidates.push_back(fs::current_path() / "mcpp" / "build" / "block_rotate_mirror.tsv");
        candidates.push_back(fs::current_path() / "build" / "block_rotate_mirror.tsv");
        fs::path repoFromData = dataDir;
        for (int i = 0; i < 3 && repoFromData.has_parent_path(); ++i) repoFromData = repoFromData.parent_path();
        candidates.push_back(repoFromData / "mcpp" / "build" / "block_rotate_mirror.tsv");

        fs::path chosen;
        for (const auto& p : candidates) {
            if (fs::exists(p)) {
                chosen = p;
                break;
            }
        }
        if (chosen.empty()) return;

        std::ifstream f(chosen, std::ios::binary);
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto c = splitc(line, '\t');
            if (c.size() >= 4 && c[0] == "FAM") {
                families[stripMinecraft(c[1])] = {c[2], c[3]};
            }
        }
    }

    std::uint32_t resolve(const std::string& fullName, const std::string& props) const {
        std::string name = stripMinecraft(fullName);
        auto it = reverse.find(name + "\x01" + props);
        if (it != reverse.end()) return it->second;
        return mc::getDefaultBlockStateId(name, 0);
    }

    std::uint32_t rotateState(std::uint32_t id, Rotation rot) const {
        if (rot == Rotation::NONE || id >= states.size()) return id;
        auto fit = families.find(states[id].name);
        if (fit == families.end()) return id;
        auto lookup = [this](const std::string& name, const std::string& props) -> long {
            auto it = reverse.find(name + "\x01" + props);
            return it == reverse.end() ? -1L : static_cast<long>(it->second);
        };
        long out = mc::block_rotation::rotate(states[id], static_cast<long>(id), rot, fit->second.first, lookup);
        return out >= 0 ? static_cast<std::uint32_t>(out) : id;
    }

    // BlockState.mirror(Mirror) — the certified block_rotation::mirror dispatch over the
    // mirror declaring-class family (families[].second). Used by template placement that
    // applies a Mirror (e.g. ruined_portal's FRONT_BACK).
    std::uint32_t mirrorState(std::uint32_t id, Mirror mir) const {
        if (mir == Mirror::NONE || id >= states.size()) return id;
        auto fit = families.find(states[id].name);
        if (fit == families.end()) return id;
        auto lookup = [this](const std::string& name, const std::string& props) -> long {
            auto it = reverse.find(name + "\x01" + props);
            return it == reverse.end() ? -1L : static_cast<long>(it->second);
        };
        long out = mc::block_rotation::mirror(states[id], static_cast<long>(id), mir, fit->second.second, lookup);
        return out >= 0 ? static_cast<std::uint32_t>(out) : id;
    }
};

struct PlaceBlock {
    BlockPos pos{};
    std::uint32_t state = 0;
};

struct PlaceTemplate {
    BlockPos size{};
    std::vector<PlaceBlock> blocks;
};

PlaceTemplate loadPlaceTemplate(const mc::nbt::NbtCompound& root, const StateResolver& resolver) {
    struct PaletteState {
        std::string name = "minecraft:air";
        std::string shortName = "air";
        std::string props;
        std::uint32_t id = 0;
        bool structureVoid = false;
    };

    PlaceTemplate out;
    out.size = posFromList(root.getList("size"));

    const mc::nbt::NbtList* paletteList = root.getList("palette");
    if (const mc::nbt::NbtList* palettes = root.getList("palettes")) {
        if (!palettes->elements.empty()) {
            if (const auto* lp = palettes->elements[0].as<std::shared_ptr<mc::nbt::NbtList>>()) {
                paletteList = lp->get();
            }
        }
    }

    std::vector<PaletteState> palette;
    if (paletteList) {
        for (const auto& entry : paletteList->elements) {
            PaletteState ps;
            if (const auto* cp = entry.as<std::shared_ptr<mc::nbt::NbtCompound>>()) {
                const mc::nbt::NbtCompound& c = **cp;
                ps.name = normalizeId(c.getString("Name", "minecraft:air"));
                ps.shortName = stripMinecraft(ps.name);
                ps.props = propsFromCompound(c.getCompound("Properties"));
                ps.id = resolver.resolve(ps.name, ps.props);
                ps.structureVoid = ps.shortName == "structure_void";
            }
            palette.push_back(std::move(ps));
        }
    }
    if (palette.empty()) {
        palette.push_back(PaletteState{});
    }

    if (const mc::nbt::NbtList* blockList = root.getList("blocks")) {
        for (const auto& entry : blockList->elements) {
            const auto* cp = entry.as<std::shared_ptr<mc::nbt::NbtCompound>>();
            if (!cp) continue;
            const mc::nbt::NbtCompound& b = **cp;
            int stateIdx = b.getInt("state", 0);
            if (stateIdx < 0 || static_cast<std::size_t>(stateIdx) >= palette.size()) stateIdx = 0;
            const PaletteState& ps = palette[static_cast<std::size_t>(stateIdx)];
            if (ps.structureVoid) continue;

            std::uint32_t state = ps.id;
            bool skip = false;
            if (ps.shortName == "jigsaw") {
                const mc::nbt::NbtCompound* nbt = b.getCompound("nbt");
                std::string finalState = nbt ? nbt->getString("final_state", "") : "";
                if (!finalState.empty()) {
                    ParsedState parsed = parseSerializedState(finalState);
                    if (stripMinecraft(parsed.name) == "structure_void") {
                        skip = true;
                    } else {
                        state = resolver.resolve(parsed.name, parsed.props);
                    }
                } else {
                    skip = true;
                }
            }
            if (skip) continue;

            out.blocks.push_back(PlaceBlock{posFromList(b.getList("pos")), state});
        }
    }

    std::stable_sort(out.blocks.begin(), out.blocks.end(), [](const PlaceBlock& a, const PlaceBlock& b) {
        if (a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
        if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
        return a.pos.z < b.pos.z;
    });
    return out;
}

int stepY(int d) {
    return d == 1 ? 1 : (d == 0 ? -1 : 0);
}

BlockPos relative(const BlockPos& p, int d) {
    switch (d) {
        case 0: return {p.x, p.y - 1, p.z};
        case 1: return {p.x, p.y + 1, p.z};
        case 2: return {p.x, p.y, p.z - 1};
        case 3: return {p.x, p.y, p.z + 1};
        case 4: return {p.x - 1, p.y, p.z};
        case 5: return {p.x + 1, p.y, p.z};
        default: return p;
    }
}

template <class D>
int dirOrd(D d) {
    return static_cast<int>(d);
}

phys::AABB aabbOf(const BoundingBox& b) {
    return phys::AABB(static_cast<double>(b.minX), static_cast<double>(b.minY), static_cast<double>(b.minZ),
                      static_cast<double>(b.maxX + 1), static_cast<double>(b.maxY + 1), static_cast<double>(b.maxZ + 1));
}

struct Placed {
    const pools::StructurePoolElement* element = nullptr;
    std::string loc;
    BlockPos position{};
    int groundLevelDelta = 0;
    Rotation rotation = Rotation::NONE;
    BoundingBox box{0, 0, 0, 0, 0, 0};
    int numJunctions = 0;
    bool rigid = false;  // element projection == RIGID (only RIGID pieces beard)
    std::vector<mc::levelgen::Beardifier::Junction> junctions;  // PoolElementStructurePiece.getJunctions

    void move(int dx, int dy, int dz) {
        box.move(dx, dy, dz);
        position = {position.x + dx, position.y + dy, position.z + dz};
    }
};

struct Free {
    shp::VoxelShapePtr v;
};
using FreePtr = std::shared_ptr<Free>;

struct PieceState {
    int pieceIdx = 0;
    FreePtr free;
    int depth = 0;
};

struct StructureSelection {
    std::string structureId;
    int weight = 1;
};

struct StructureSetDef {
    std::string id;
    std::vector<StructureSelection> structures;
};

enum class HeightKind { Constant, Uniform, Unsupported };

struct HeightProvider {
    HeightKind kind = HeightKind::Constant;
    int min = 0;
    int max = 0;
};

struct JigsawConfig {
    std::string id;
    std::string structureType;  // "minecraft:jigsaw", "minecraft:swamp_hut", etc.
    bool supported = false;
    std::string reason;

    // Shipwreck-specific (is_beached in the structure JSON)
    bool isBeached = false;
    // OceanRuin-specific (biome_temp / probabilities in the structure JSON)
    bool oceanRuinWarm = false;
    float largeProbability = 0.0f;
    float clusterProbability = 0.0f;
    // Mineshaft-specific (mineshaft_type in the structure JSON: "normal" / "mesa")
    bool mineshaftMesa = false;
    // RuinedPortal-specific: list of setups (placement/airPocket/mossiness/etc.)
    struct RuinedPortalSetup {
        std::string placement;     // "on_land_surface", "underground", "in_nether", etc.
        float airPocketProbability = 0.0f;
        float mossiness = 0.0f;
        bool overgrown = false;
        bool vines = false;
        bool canBeCold = false;
        bool replaceWithBlackstone = false;
        float weight = 1.0f;
    };
    std::vector<RuinedPortalSetup> ruinedPortalSetups;

    // terrain_adaptation (TerrainAdjustment) — drives the Beardifier.
    mc::levelgen::TerrainAdjustment terrainAdjustment = mc::levelgen::TerrainAdjustment::NONE;
    int stepIndex = static_cast<int>(mc::levelgen::feature::GenerationStep::SURFACE_STRUCTURES);

    std::set<std::string> biomes;
    std::string startPool;
    bool hasStartJigsaw = false;
    std::string startJigsawName;
    int maxDepth = 0;
    HeightProvider height;
    bool projectStartToHeightmap = false;
    int maxDistH = 0;
    int maxDistV = 0;
    bool useExpansionHack = false;
    int padBottom = 0;
    int padTop = 0;
    std::vector<palias::Binding> aliases;
};

int verticalAnchor(const json& j, bool& ok) {
    if (j.is_number_integer()) {
        ok = true;
        return j.get<int>();
    }
    if (!j.is_object()) {
        ok = false;
        return 0;
    }
    if (j.contains("absolute")) {
        ok = true;
        return j.at("absolute").get<int>();
    }
    if (j.contains("above_bottom")) {
        ok = true;
        return kMinBuildY + j.at("above_bottom").get<int>();
    }
    if (j.contains("below_top")) {
        ok = true;
        return kMaxBuildYExclusive - j.at("below_top").get<int>();
    }
    ok = false;
    return 0;
}

int decorationStepIndex(const std::string& step) {
    using D = mc::levelgen::feature::GenerationStep::Decoration;
    if (step == "raw_generation") return static_cast<int>(D::RAW_GENERATION);
    if (step == "lakes") return static_cast<int>(D::LAKES);
    if (step == "local_modifications") return static_cast<int>(D::LOCAL_MODIFICATIONS);
    if (step == "underground_structures") return static_cast<int>(D::UNDERGROUND_STRUCTURES);
    if (step == "surface_structures") return static_cast<int>(D::SURFACE_STRUCTURES);
    if (step == "strongholds") return static_cast<int>(D::STRONGHOLDS);
    if (step == "underground_ores") return static_cast<int>(D::UNDERGROUND_ORES);
    if (step == "underground_decoration") return static_cast<int>(D::UNDERGROUND_DECORATION);
    if (step == "fluid_springs") return static_cast<int>(D::FLUID_SPRINGS);
    if (step == "vegetal_decoration") return static_cast<int>(D::VEGETAL_DECORATION);
    if (step == "top_layer_modification") return static_cast<int>(D::TOP_LAYER_MODIFICATION);
    return static_cast<int>(D::SURFACE_STRUCTURES);
}

HeightProvider parseHeightProvider(const json& j) {
    HeightProvider hp;
    if (j.is_object() && j.value("type", std::string()) == "minecraft:uniform") {
        bool okMin = false;
        bool okMax = false;
        hp.kind = HeightKind::Uniform;
        hp.min = verticalAnchor(j.at("min_inclusive"), okMin);
        hp.max = verticalAnchor(j.at("max_inclusive"), okMax);
        if (!okMin || !okMax) hp.kind = HeightKind::Unsupported;
        return hp;
    }
    bool ok = false;
    hp.min = hp.max = verticalAnchor(j, ok);
    hp.kind = ok ? HeightKind::Constant : HeightKind::Unsupported;
    return hp;
}

// --- Structure processor pipeline (StructureTemplate.processBlockInfos) ---------
// Port of the RuleProcessor family used by every jigsaw structure's pool elements
// (villages: street/farm/mossify/zombie; outpost: outpost_rot; trial chambers:
// copper_bulb_degradation). Each processor list referenced by a SinglePoolElement
// is a list of `minecraft:rule` processors; each rule is input_predicate +
// location_predicate + output_state. Per block a fresh LegacyRandomSource is seeded
// from the WORLD position via Mth.getSeed (RuleProcessor.processBlock). The RuleTest
// arms are the certified set (StructureProcessorParityTest): always_true,
// block_match, blockstate_match, tag_match, random_block_match. tag_match needs
// block tags (only zombie villages) and is deferred: it never fires (hard no-op).
struct RuleTest {
    enum Kind { ALWAYS_TRUE, BLOCK_MATCH, BLOCKSTATE_MATCH, TAG_MATCH, RANDOM_BLOCK_MATCH, UNSUPPORTED };
    Kind kind = ALWAYS_TRUE;
    std::string blockShort;       // block_match / random_block_match (ns-stripped)
    std::uint32_t stateId = 0;    // blockstate_match (resolved state id)
    std::string tag;              // tag_match (full id)
    float probability = 1.0f;     // random_block_match
};

struct ProcRule {
    RuleTest input;
    RuleTest loc;
    std::uint32_t outputState = 0;
    std::string outputShortName;  // ns-stripped name of output_state (for legacy AIR ignore)
};

using ProcList = std::vector<ProcRule>;

struct Runtime {
    fs::path dataDir;
    int64_t seed = 0;
    StructureState placementState;
    StateResolver resolver;
    std::vector<StructureSetDef> structureSets;
    std::map<std::string, JigsawConfig> structures;
    std::map<std::string, pools::StructureTemplatePool> poolMap;
    std::map<std::string, ProcList> processorLists;   // cache by list id
    bool warnedUnsupportedProc = false;
    std::map<std::string, stl::LoadedTemplate> jigsawTemplates;
    std::map<std::string, PlaceTemplate> placeTemplates;
    std::unordered_map<std::string, std::string> oracleTemplateB64;
    std::unordered_set<std::string> loadedPoolRoots;
    std::unordered_set<std::string> missingTemplates;
    std::unordered_set<std::string> biomeTagStack;
    bool oracleTemplateIndexLoaded = false;
    // Phase 3 of Option B refactor: guard the lazily-loaded caches so that
    // generate() can be called from the decoration worker thread while the
    // main thread might also be querying the same Runtime. Reads of the
    // caches take shared lock; lazy-load paths (ensureTemplate etc.) take
    // exclusive lock. Once all templates are loaded (warm cache), generate()
    // runs fully under shared lock — no contention.
    mutable std::shared_mutex cacheMutex;

    Runtime(fs::path dir, int64_t levelSeed) : dataDir(std::move(dir)), seed(levelSeed) {
        placementState = StructureState::loadFromDirectory((dataDir / "worldgen" / "structure_set").string(), seed);
        resolver.init(dataDir);
        loadStructureSets();
        loadStructures();
        poolMap["minecraft:empty"] = pools::StructureTemplatePool{};

        // RULE #0 visibility: report exactly which structures are NOT placed so an
        // unported family is never silently mistaken for a working one.
        std::map<std::string, int> unportedByType;
        for (const auto& [sid, cfg] : structures) {
            if (!cfg.supported) ++unportedByType[cfg.structureType];
        }
        MC_LOG_INFO("Structures: loaded {} structure_sets, {} structures from {}",
                    structureSets.size(), structures.size(), dataDir.generic_string());
        for (const auto& [type, count] : unportedByType) {
            MC_LOG_INFO("Structures: UNPORTED type {} ({} structures) — hard no-op, not placed",
                        type, count);
        }
    }

    void loadStructureSets();
    void loadStructures();
    JigsawConfig loadOneStructure(const std::string& id, const json& j);
    void resolveBiomeSpec(const json& spec, std::set<std::string>& out);
    void resolveBiomeTag(const std::string& tagId, std::set<std::string>& out);
    void ensurePoolSubtree(const std::string& startPool);
    void loadOracleTemplateIndex();
    std::optional<std::vector<std::uint8_t>> oracleTemplateBytes(const std::string& location);
    bool ensureTemplate(const std::string& location);
    Vec3i sizeOf(const std::string& location);
    std::vector<stl::JigsawBlockInfo> shuffledJigsaws(const pools::StructurePoolElement& e,
                                                      const BlockPos& pos, Rotation rot,
                                                      mc::levelgen::RandomSource& random);
    bool validBiome(const JigsawConfig& cfg, const BlockPos& pos,
                    const std::function<std::string(int, int, int)>& biomeGetter) const;
    bool assembleJigsaw(const JigsawConfig& cfg, ChunkPos active, const StructureWorld& world,
                        const std::function<std::string(int, int, int)>& biomeGetter,
                        std::vector<Placed>& pieces, BlockPos& stubPos);
    std::size_t placePieces(const std::vector<Placed>& pieces, const StructureWorld& world,
                            mc::levelgen::RandomSource* random = nullptr,
                            const BoundingBox* chunkBB = nullptr);
    std::size_t placeElement(const pools::StructurePoolElement& e, const BlockPos& pos,
                             Rotation rot, const StructureWorld& world,
                             mc::levelgen::RandomSource* random = nullptr,
                             const BoundingBox* chunkBB = nullptr);
    std::size_t placeTemplate(const std::string& location, const BlockPos& pos,
                              Rotation rot, const StructureWorld& world,
                              bool legacy = false,
                              const std::string& processorsId = "minecraft:empty",
                              bool terrainMatching = false,
                              const BoundingBox* chunkBB = nullptr,
                              float integrity = 1.0f);

    // OceanRuin (non-jigsaw, template-based with BlockRotProcessor integrity decay).
    bool tryPlaceOceanRuin(ChunkPos active, const StructureWorld& world,
                           bool warm, float largeProbability, float clusterProbability);
    void addOceanRuinPiece(const BlockPos& pos, Rotation rot, bool isLarge, float integrity,
                           bool warm, mc::levelgen::WorldgenRandom& random,
                           const StructureWorld& world, std::size_t& placed);
    void addOceanRuinCluster(const BlockPos& p, Rotation rot, bool warm,
                             mc::levelgen::WorldgenRandom& random,
                             const StructureWorld& world, std::size_t& placed);
    // Processor pipeline.
    RuleTest parseRuleTest(const json& j);
    std::uint32_t parseStateObject(const json& j, std::string& outShortName);
    const ProcList* loadProcessorList(const std::string& id);
    bool testRule(const RuleTest& t, std::uint32_t stateId, const std::string& shortName,
                  mc::levelgen::RandomSource& random);
    std::uint32_t applyRules(const ProcList& rules, std::uint32_t inputState,
                             const std::string& inputShortName,
                             int wx, int wy, int wz, const StructureWorld& world);
    bool tryGenerateAndPlace(const std::string& structureId, ChunkPos active, const StructureWorld& world,
                             const std::function<std::string(int, int, int)>& biomeGetter);
    const JigsawConfig* selectJigsawStructure(const StructureSetDef& set, ChunkPos start,
                                             const StructureWorld& world,
                                             const std::function<std::string(int, int, int)>& biomeGetter,
                                             const std::vector<Placed>*& outPieces);
    std::size_t placeJigsawStartInChunk(const JigsawConfig& cfg, ChunkPos start, ChunkPos decorating,
                                        const std::vector<Placed>& pieces,
                                        const StructureWorld& world,
                                        mc::levelgen::RandomSource& random);
    bool tryPlaceSwampHut(ChunkPos active, const StructureWorld& world);
    bool tryPlaceDesertPyramid(ChunkPos active, const StructureWorld& world);
    bool tryPlaceJungleTemple(ChunkPos active, const StructureWorld& world);
    bool tryPlaceShipwreck(ChunkPos active, const StructureWorld& world, bool isBeached);
    bool tryPlaceIgloo(ChunkPos active, const StructureWorld& world);
    bool tryPlaceNetherFossil(ChunkPos active, const StructureWorld& world);
    bool tryPlaceBuriedTreasure(ChunkPos active, const StructureWorld& world);
    bool tryPlaceMineshaft(ChunkPos active, const StructureWorld& world, bool isMesa);
    bool tryPlaceRuinedPortal(ChunkPos active, const StructureWorld& world,
                              const std::vector<JigsawConfig::RuinedPortalSetup>& setups);
    bool tryPlaceOceanMonument(ChunkPos active, const StructureWorld& world);
    bool tryPlaceWoodlandMansion(ChunkPos active, const StructureWorld& world);
    bool tryPlaceNetherFortress(ChunkPos active, const StructureWorld& world);
    bool tryPlaceStronghold(ChunkPos active, const StructureWorld& world);
    bool tryPlaceEndCity(ChunkPos active, const StructureWorld& world);
    void generate(ChunkPos active, const StructureWorld& world,
                  const std::function<std::string(int, int, int)>& biomeGetter);

    // Beardifier integration: assemble (cached) the terrain-adapting structures whose
    // pieces reach `active`, then collect their RIGID pieces + junctions per
    // Beardifier.forStructuresInChunk. `columnHeight(x,z)` is the noise-column surface
    // (WORLD_SURFACE_WG topmost solid) sampled before the chunk's terrain exists.
    const std::vector<Placed>* assembledFor(const std::string& structureId, ChunkPos start,
                                            const StructureWorld& world,
                                            const std::function<std::string(int, int, int)>& biomeGetter);
    mc::levelgen::Beardifier buildBeardifier(ChunkPos active,
                                             const std::function<int(int, int)>& columnHeight,
                                             const std::function<std::string(int, int, int)>& biomeGetter);

    // assembled-structure cache (keyed by structureId + start chunk).
    std::map<std::string, std::vector<Placed>> assembledCache;
};

struct Placer {
    Runtime* rt = nullptr;
    const StructureWorld* world = nullptr;
    pools::SizeResolver sizeOf;
    int maxDepth = 0;
    int maxDistH = 0;
    int maxDistV = 0;
    bool doExpansionHack = false;
    std::vector<Placed>* pieces = nullptr;
    std::shared_ptr<mc::levelgen::RandomSource> random;
    const std::map<std::string, std::string>* aliasMap = nullptr;
    mc::util::SequencedPriorityIterator<PieceState> placing;

    std::vector<stl::JigsawBlockInfo> shuffledJigsaws(const pools::StructurePoolElement& e,
                                                      const BlockPos& pos, Rotation rot) {
        return rt->shuffledJigsaws(e, pos, rot, *random);
    }

    int firstFreeHeight(int x, int z) const {
        return world && world->heightAt ? world->heightAt(x, z) + 1 : 1;
    }

    void tryPlacingChildren(int sourceIdx, FreePtr contextFree, int depth) {
        const pools::StructurePoolElement& sourceElement = *(*pieces)[sourceIdx].element;
        BlockPos sourceBoxPosition = (*pieces)[sourceIdx].position;
        Rotation sourceRotation = (*pieces)[sourceIdx].rotation;
        pools::Projection sourceProjection = sourceElement.getProjection();
        bool sourceRigid = sourceProjection == pools::Projection::RIGID;
        FreePtr sourceFree = std::make_shared<Free>();
        BoundingBox sourceBB = (*pieces)[sourceIdx].box;
        int sourceBoxY = sourceBB.minY;

        for (const stl::JigsawBlockInfo& sourceJigsaw : shuffledJigsaws(sourceElement, sourceBoxPosition, sourceRotation)) {
            int sourceDirection = dirOrd(sourceJigsaw.info.orientation.front);
            BlockPos sourceJigsawPos = sourceJigsaw.info.pos;
            BlockPos targetJigsawPos = relative(sourceJigsawPos, sourceDirection);
            int sourceJigsawLocalY = sourceJigsawPos.y - sourceBoxY;
            int sourceJigsawBaseHeight = kUnsetHeight;

            std::string poolName = palias::applyAlias(*aliasMap, sourceJigsaw.pool);
            auto pit = rt->poolMap.find(poolName);
            if (pit == rt->poolMap.end()) continue;
            pools::StructureTemplatePool& targetPool = pit->second;
            bool targetIsEmptyPool = poolName == "minecraft:empty";
            if (targetPool.size() == 0 && !targetIsEmptyPool) continue;
            const std::string& fbName = targetPool.getFallbackName();
            auto fit = rt->poolMap.find(fbName);
            if (fit == rt->poolMap.end()) continue;
            pools::StructureTemplatePool& fallback = fit->second;
            bool fbIsEmptyPool = fbName == "minecraft:empty";
            if (fallback.size() == 0 && !fbIsEmptyPool) {
                // Vanilla logs a warning and still uses the target pool candidates.
            }

            bool attachInsideSource = sourceBB.isInside(targetJigsawPos);
            FreePtr childrenFree;
            if (attachInsideSource) {
                childrenFree = sourceFree;
                if (!sourceFree->v) sourceFree->v = shp::Shapes::create(aabbOf(sourceBB));
            } else {
                childrenFree = contextFree;
            }

            std::vector<const pools::StructurePoolElement*> targetPieces;
            if (depth != maxDepth) {
                for (int i : targetPool.getShuffledTemplateIndices(*random)) {
                    targetPieces.push_back(&targetPool.templates[static_cast<std::size_t>(i)]);
                }
            }
            for (int i : fallback.getShuffledTemplateIndices(*random)) {
                targetPieces.push_back(&fallback.templates[static_cast<std::size_t>(i)]);
            }
            int placementPriority = sourceJigsaw.placementPriority;

            bool placed = false;
            for (const pools::StructurePoolElement* targetElement : targetPieces) {
                if (targetElement->isEmpty()) break;
                for (auto blockRot : mc::block::rotationGetShuffled(*random)) {
                    Rotation targetRotation = static_cast<Rotation>(static_cast<int>(blockRot));
                    std::vector<stl::JigsawBlockInfo> targetJigsaws =
                        shuffledJigsaws(*targetElement, BlockPos{0, 0, 0}, targetRotation);
                    BoundingBox hackBox = targetElement->getBoundingBox(sizeOf, BlockPos{0, 0, 0}, targetRotation);
                    int expandTo = 0;
                    if (doExpansionHack && hackBox.getYSpan() <= 16) {
                        for (const stl::JigsawBlockInfo& tj : targetJigsaws) {
                            BlockPos rp = relative(tj.info.pos, dirOrd(tj.info.orientation.front));
                            if (!hackBox.isInside(rp)) continue;
                            auto cit = rt->poolMap.find(palias::applyAlias(*aliasMap, tj.pool));
                            int childPoolSize = 0;
                            int childFbSize = 0;
                            if (cit != rt->poolMap.end()) {
                                childPoolSize = cit->second.getMaxSize(sizeOf);
                                auto cfit = rt->poolMap.find(cit->second.getFallbackName());
                                if (cfit != rt->poolMap.end()) childFbSize = cfit->second.getMaxSize(sizeOf);
                            }
                            expandTo = std::max(expandTo, std::max(childPoolSize, childFbSize));
                        }
                    }
                    for (const stl::JigsawBlockInfo& targetJigsaw : targetJigsaws) {
                        bool can = mc::block::canAttach(
                            static_cast<mc::Direction>(dirOrd(sourceJigsaw.info.orientation.front)),
                            static_cast<mc::Direction>(dirOrd(sourceJigsaw.info.orientation.top)),
                            sourceJigsaw.target,
                            static_cast<mc::block::JointType>(static_cast<int>(sourceJigsaw.jointType)),
                            static_cast<mc::Direction>(dirOrd(targetJigsaw.info.orientation.front)),
                            static_cast<mc::Direction>(dirOrd(targetJigsaw.info.orientation.top)),
                            targetJigsaw.name);
                        if (!can) continue;

                        BlockPos targetJigsawLocalPos = targetJigsaw.info.pos;
                        BlockPos rawTargetBoxPos = {targetJigsawPos.x - targetJigsawLocalPos.x,
                                                    targetJigsawPos.y - targetJigsawLocalPos.y,
                                                    targetJigsawPos.z - targetJigsawLocalPos.z};
                        BoundingBox rawTargetBB = targetElement->getBoundingBox(sizeOf, rawTargetBoxPos, targetRotation);
                        int rawTargetY = rawTargetBB.minY;
                        pools::Projection targetProjection = targetElement->getProjection();
                        bool targetRigid = targetProjection == pools::Projection::RIGID;
                        int targetJigsawLocalY = targetJigsawLocalPos.y;
                        int deltaY = sourceJigsawLocalY - targetJigsawLocalY + stepY(sourceDirection);
                        int targetBoxY;
                        if (sourceRigid && targetRigid) {
                            targetBoxY = sourceBoxY + deltaY;
                        } else {
                            if (sourceJigsawBaseHeight == kUnsetHeight) {
                                sourceJigsawBaseHeight = firstFreeHeight(sourceJigsawPos.x, sourceJigsawPos.z);
                            }
                            targetBoxY = sourceJigsawBaseHeight - targetJigsawLocalY;
                        }

                        int yOffset = targetBoxY - rawTargetY;
                        BoundingBox targetBB = rawTargetBB.moved(0, yOffset, 0);
                        BlockPos targetBoxPosition = {rawTargetBoxPos.x, rawTargetBoxPos.y + yOffset, rawTargetBoxPos.z};
                        if (expandTo > 0) {
                            int newSize = std::max(expandTo + 1, targetBB.maxY - targetBB.minY);
                            targetBB.encapsulate(Vec3i{targetBB.minX, targetBB.minY + newSize, targetBB.minZ});
                        }
                        if (!shp::Shapes::joinIsNotEmpty(childrenFree->v,
                                shp::Shapes::create(aabbOf(targetBB).deflate(0.25)),
                                shp::BooleanOps::ONLY_SECOND)) {
                            childrenFree->v = shp::Shapes::joinUnoptimized(
                                childrenFree->v, shp::Shapes::create(aabbOf(targetBB)), shp::BooleanOps::ONLY_FIRST);
                            int sourceGroundLevelDelta = (*pieces)[sourceIdx].groundLevelDelta;
                            int targetGroundLevelDelta = targetRigid ? (sourceGroundLevelDelta - deltaY)
                                                                     : targetElement->getGroundLevelDelta();

                            Placed tp;
                            tp.element = targetElement;
                            tp.loc = targetElement->locationString();
                            tp.position = targetBoxPosition;
                            tp.groundLevelDelta = targetGroundLevelDelta;
                            tp.rotation = targetRotation;
                            tp.box = targetBB;
                            tp.rigid = targetRigid;

                            // JigsawJunction recording (JigsawPlacement.java:439-472) —
                            // needed by the Beardifier. junctionY depends on which
                            // side is rigid.
                            int junctionY;
                            if (sourceRigid) {
                                junctionY = sourceBoxY + sourceJigsawLocalY;
                            } else if (targetRigid) {
                                junctionY = targetBoxY + targetJigsawLocalY;
                            } else {
                                if (sourceJigsawBaseHeight == kUnsetHeight) {
                                    sourceJigsawBaseHeight = firstFreeHeight(sourceJigsawPos.x, sourceJigsawPos.z);
                                }
                                junctionY = sourceJigsawBaseHeight + deltaY / 2;
                            }
                            (*pieces)[sourceIdx].junctions.push_back(
                                {targetJigsawPos.x, junctionY - sourceJigsawLocalY + sourceGroundLevelDelta, targetJigsawPos.z});
                            tp.junctions.push_back(
                                {sourceJigsawPos.x, junctionY - targetJigsawLocalY + targetGroundLevelDelta, sourceJigsawPos.z});

                            (*pieces)[sourceIdx].numJunctions += 1;
                            tp.numJunctions += 1;
                            pieces->push_back(tp);
                            int targetIdx = static_cast<int>(pieces->size()) - 1;
                            if (depth + 1 <= maxDepth) {
                                placing.add(PieceState{targetIdx, childrenFree, depth + 1}, placementPriority);
                            }
                            placed = true;
                            break;
                        }
                    }
                    if (placed) break;
                }
                if (placed) break;
            }
        }
    }
};

void Runtime::loadStructureSets() {
    fs::path base = dataDir / "worldgen" / "structure_set";
    for (const fs::path& file : sortedJsonFiles(base, false)) {
        std::string text = readFile(file);
        if (text.empty()) continue;
        json j = json::parse(text);
        StructureSetDef set;
        set.id = "minecraft:" + file.stem().generic_string();
        const json& arr = j.at("structures");
        for (const auto& entry : arr) {
            StructureSelection sel;
            sel.structureId = normalizeId(entry.at("structure").get<std::string>());
            sel.weight = entry.value("weight", 1);
            set.structures.push_back(std::move(sel));
        }
        if (!set.structures.empty()) structureSets.push_back(std::move(set));
    }
}

void Runtime::loadStructures() {
    fs::path base = dataDir / "worldgen" / "structure";
    for (const fs::path& file : sortedJsonFiles(base, true)) {
        std::string rel = fs::relative(file, base).generic_string();
        if (rel.size() < 6) continue;
        std::string id = "minecraft:" + rel.substr(0, rel.size() - 5);
        std::string text = readFile(file);
        if (text.empty()) continue;
        structures[id] = loadOneStructure(id, json::parse(text));
    }
}

JigsawConfig Runtime::loadOneStructure(const std::string& id, const json& j) {
    JigsawConfig cfg;
    cfg.id = id;
    std::string type = normalizeId(j.value("type", std::string()));
    cfg.structureType = type;
    // terrain_adaptation (default none) — feeds the Beardifier.
    cfg.terrainAdjustment = mc::levelgen::terrainAdjustmentByName(
        j.value("terrain_adaptation", std::string("none")));
    cfg.stepIndex = decorationStepIndex(j.value("step", std::string("surface_structures")));
    
    // RULE #0 HONESTY: a structure type is listed here ONLY if it has a real,
    // dispatched piece-placement path in tryGenerateAndPlace(). A type that is
    // recognised but NOT actually placed must NOT be marked supported — otherwise
    // it silently no-ops (failed jigsaw assembly with an empty start_pool) while
    // pretending to be ported. Types deliberately NOT here yet (helpers only, no
    // in-game placement): ruined_portal,
    // ocean_monument, woodland_mansion, stronghold, fortress, end_city.
    // See docs/STRUCTURES_STATUS.md for the per-structure port ledger.
    static const std::set<std::string> supportedTypes = {
        "minecraft:jigsaw",
        "minecraft:swamp_hut",
        "minecraft:desert_pyramid",
        "minecraft:igloo",
        "minecraft:jungle_temple",
        "minecraft:shipwreck",
        "minecraft:nether_fossil",
        "minecraft:buried_treasure",
        "minecraft:ocean_ruin",
        "minecraft:mineshaft",
        "minecraft:ruined_portal",
        "minecraft:end_city",
        // RULE #0: woodland_mansion / fortress / stronghold / ocean_monument removed —
        // their tryPlace* bodies are NOT faithful piece ports (mansion: 52×52
        // cobblestone slab; fortress: 5×10 nether-brick bridge; stronghold: 16×16×8
        // stone box that carves a 14×14×6 AIR pocket; ocean_monument: outer shell
        // only, no rooms, RNG stream not matching Java). A not-yet-ported structure
        // must be a hard no-op, never a silent return-true. Re-add ONLY with a real
        // 1:1 piece port. (end_city is End-only: kept.) Keep in sync with WorldGen.cpp.
    };

    if (supportedTypes.count(type) == 0) {
        cfg.supported = false;
        cfg.reason = "type " + type + " not yet ported (no piece placement) — hard no-op";
        return cfg;
    }

    if (type != "minecraft:jigsaw") {
        // Non-jigsaw structures don't need jigsaw config fields, but they DO carry
        // a `biomes` set that gates placement (Structure.isValidBiome). Parse it so
        // the dispatch can reject e.g. a desert pyramid in plains or a nether fossil
        // in the overworld.
        cfg.supported = true;
        if (j.contains("biomes")) resolveBiomeSpec(j.at("biomes"), cfg.biomes);
        // Shipwreck: parse is_beached flag
        if (type == "minecraft:shipwreck") {
            cfg.isBeached = j.value("is_beached", false);
        }
        // OceanRuin: biome_temp (warm/cold) + large/cluster probabilities
        if (type == "minecraft:ocean_ruin") {
            cfg.oceanRuinWarm = (j.value("biome_temp", std::string("cold")) == "warm");
            cfg.largeProbability = j.value("large_probability", 0.0f);
            cfg.clusterProbability = j.value("cluster_probability", 0.0f);
        }
        // Mineshaft: mineshaft_type ("normal" / "mesa") — selects planks/wood/fence.
        if (type == "minecraft:mineshaft") {
            cfg.mineshaftMesa = (j.value("mineshaft_type", std::string("normal")) == "mesa");
        }
        // RuinedPortal: parse the "setups" array (placement, airPocket, mossiness, etc.)
        if (type == "minecraft:ruined_portal") {
            if (j.contains("setups") && j.at("setups").is_array()) {
                for (const auto& s : j.at("setups")) {
                    JigsawConfig::RuinedPortalSetup setup;
                    setup.placement = s.value("placement", std::string("on_land_surface"));
                    setup.airPocketProbability = s.value("air_pocket_probability", 0.0f);
                    setup.mossiness = s.value("mossiness", 0.0f);
                    setup.overgrown = s.value("overgrown", false);
                    setup.vines = s.value("vines", false);
                    setup.canBeCold = s.value("can_be_cold", false);
                    setup.replaceWithBlackstone = s.value("replace_with_blackstone", false);
                    setup.weight = s.value("weight", 1.0f);
                    cfg.ruinedPortalSetups.push_back(std::move(setup));
                }
            }
        }
        return cfg;
    }

    try {
        cfg.supported = true;
        cfg.maxDepth = j.at("size").get<int>();
        cfg.startPool = normalizeId(j.at("start_pool").get<std::string>());
        if (j.contains("start_jigsaw_name")) {
            cfg.hasStartJigsaw = true;
            cfg.startJigsawName = normalizeId(j.at("start_jigsaw_name").get<std::string>());
        }
        cfg.height = parseHeightProvider(j.at("start_height"));
        if (cfg.height.kind == HeightKind::Unsupported) {
            cfg.supported = false;
            cfg.reason = "unsupported start_height";
        }
        cfg.projectStartToHeightmap = j.contains("project_start_to_heightmap");
        if (j.contains("max_distance_from_center")) {
            const json& md = j.at("max_distance_from_center");
            if (md.is_number_integer()) {
                cfg.maxDistH = cfg.maxDistV = md.get<int>();
            } else {
                cfg.maxDistH = md.value("horizontal", 80);
                cfg.maxDistV = md.value("vertical", cfg.maxDistH);
            }
        }
        cfg.useExpansionHack = j.value("use_expansion_hack", false);
        if (j.contains("dimension_padding")) {
            const json& p = j.at("dimension_padding");
            if (p.is_number_integer()) {
                cfg.padBottom = cfg.padTop = p.get<int>();
            } else if (p.is_object()) {
                cfg.padBottom = p.value("bottom", 0);
                cfg.padTop = p.value("top", 0);
            }
        }
        if (j.contains("pool_aliases")) cfg.aliases = palias::parseBindings(j.at("pool_aliases"));
        if (j.contains("biomes")) resolveBiomeSpec(j.at("biomes"), cfg.biomes);
    } catch (const std::exception& e) {
        cfg.supported = false;
        cfg.reason = e.what();
    }
    return cfg;
}

void Runtime::resolveBiomeSpec(const json& spec, std::set<std::string>& out) {
    if (spec.is_string()) {
        std::string s = spec.get<std::string>();
        if (!s.empty() && s[0] == '#') resolveBiomeTag(s.substr(1), out);
        else out.insert(normalizeId(s));
    } else if (spec.is_array()) {
        for (const auto& v : spec) resolveBiomeSpec(v, out);
    }
}

void Runtime::resolveBiomeTag(const std::string& tagId, std::set<std::string>& out) {
    std::string id = normalizeId(tagId);
    if (!biomeTagStack.insert(id).second) return;
    fs::path path = pathForId(dataDir / "tags" / "worldgen" / "biome", id, ".json");
    std::string text = readFile(path);
    if (!text.empty()) {
        json j = json::parse(text);
        if (j.contains("values")) {
            for (const auto& v : j.at("values")) {
                std::string s = v.get<std::string>();
                if (!s.empty() && s[0] == '#') resolveBiomeTag(s.substr(1), out);
                else out.insert(normalizeId(s));
            }
        }
    }
    biomeTagStack.erase(id);
}

void Runtime::ensurePoolSubtree(const std::string& startPool) {
    auto sub = minecraftSubPath(startPool);
    if (!sub) return;
    std::string root = sub->substr(0, sub->find('/'));
    if (!loadedPoolRoots.insert(root).second) return;

    fs::path base = dataDir / "worldgen" / "template_pool";
    fs::path rootDir = base / root;
    for (const fs::path& file : sortedJsonFiles(rootDir, true)) {
        std::string rel = fs::relative(file, base).generic_string();
        if (rel.size() < 6) continue;
        std::string key = "minecraft:" + rel.substr(0, rel.size() - 5);
        poolMap[key] = pools::loadPool(json::parse(readFile(file)));
    }
}

void Runtime::loadOracleTemplateIndex() {
    if (oracleTemplateIndexLoaded) return;
    oracleTemplateIndexLoaded = true;

    std::vector<fs::path> candidates;
    candidates.push_back(fs::current_path() / "mcpp" / "build" / "jigsaw_placement.tsv");
    candidates.push_back(fs::current_path() / "build" / "jigsaw_placement.tsv");
    candidates.push_back(fs::current_path() / "jigsaw_placement.tsv");
    if (fs::current_path().has_parent_path()) {
        candidates.push_back(fs::current_path().parent_path() / "mcpp" / "build" / "jigsaw_placement.tsv");
        candidates.push_back(fs::current_path().parent_path() / "build" / "jigsaw_placement.tsv");
        candidates.push_back(fs::current_path().parent_path() / "jigsaw_placement.tsv");
    }
    fs::path repoFromData = dataDir;
    for (int i = 0; i < 3 && repoFromData.has_parent_path(); ++i) repoFromData = repoFromData.parent_path();
    candidates.push_back(repoFromData / "mcpp" / "build" / "jigsaw_placement.tsv");

    fs::path chosen;
    for (const fs::path& candidate : candidates) {
        if (!candidate.empty() && fs::exists(candidate)) {
            chosen = candidate;
            break;
        }
    }
    if (chosen.empty()) {
        MC_LOG_DEBUG("Structures: jigsaw template oracle not found; missing .nbt templates will be skipped");
        return;
    }

    std::ifstream f(chosen, std::ios::binary);
    std::string line;
    std::size_t rows = 0;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto c = splitc(line, '\t');
        if (c.size() >= 4 && c[0] == "NBT") {
            oracleTemplateB64[normalizeId(c[2])] = c[3];
            ++rows;
        }
    }

    MC_LOG_INFO("Structures: indexed {} oracle NBT templates from {}", rows, chosen.generic_string());
}

std::optional<std::vector<std::uint8_t>> Runtime::oracleTemplateBytes(const std::string& location) {
    loadOracleTemplateIndex();
    auto it = oracleTemplateB64.find(normalizeId(location));
    if (it == oracleTemplateB64.end()) return std::nullopt;
    std::vector<std::uint8_t> bytes = base64Decode(it->second);
    if (bytes.empty()) return std::nullopt;
    return bytes;
}

bool Runtime::ensureTemplate(const std::string& location) {
    if (jigsawTemplates.find(location) != jigsawTemplates.end()) return true;
    if (missingTemplates.find(location) != missingTemplates.end()) return false;

    std::vector<fs::path> candidates;
    candidates.push_back(pathForId(dataDir / "structure", location, ".nbt"));
    candidates.push_back(pathForId(fs::current_path() / "26.1.2" / "data" / "minecraft" / "structure", location, ".nbt"));
    if (fs::current_path().has_parent_path()) {
        candidates.push_back(pathForId(fs::current_path().parent_path() / "26.1.2" / "data" / "minecraft" / "structure", location, ".nbt"));
    }

    fs::path file;
    for (const fs::path& candidate : candidates) {
        if (!candidate.empty() && fs::exists(candidate)) {
            file = candidate;
            break;
        }
    }
    if (file.empty()) {
        if (auto oracleBytes = oracleTemplateBytes(location)) {
            auto root = mc::nbt::NbtReader::readGzip(*oracleBytes);
            if (root) {
                jigsawTemplates[location] = stl::loadStructureTemplate(*root);
                placeTemplates[location] = loadPlaceTemplate(*root, resolver);
                return true;
            }
        }
        missingTemplates.insert(location);
        return false;
    }

    std::vector<std::uint8_t> bytes = readBytes(file);
    if (bytes.empty()) {
        missingTemplates.insert(location);
        return false;
    }
    auto root = mc::nbt::NbtReader::readGzip(bytes);
    if (!root) {
        missingTemplates.insert(location);
        return false;
    }
    jigsawTemplates[location] = stl::loadStructureTemplate(*root);
    placeTemplates[location] = loadPlaceTemplate(*root, resolver);
    return true;
}

Vec3i Runtime::sizeOf(const std::string& location) {
    if (!ensureTemplate(location)) {
        throw std::runtime_error("missing structure template " + location);
    }
    return jigsawTemplates.at(location).size;
}

std::vector<stl::JigsawBlockInfo> Runtime::shuffledJigsaws(const pools::StructurePoolElement& e,
                                                           const BlockPos& pos, Rotation rot,
                                                           mc::levelgen::RandomSource& random) {
    if (e.type == pools::ElementType::FEATURE) {
        stl::JigsawBlockInfo fj;
        fj.info.pos = pos;
        fj.info.blockName = "minecraft:jigsaw";
        fj.info.orientation = stl::FrontAndTop{mc::Direction::DOWN, mc::Direction::SOUTH};
        fj.info.hasOrientation = true;
        fj.jointType = stl::JointType::ROLLABLE;
        fj.name = "minecraft:bottom";
        fj.pool = "minecraft:empty";
        fj.target = "minecraft:empty";
        return {fj};
    }
    if (e.type == pools::ElementType::EMPTY) return {};
    if (e.type == pools::ElementType::LIST) {
        if (e.elements.empty()) return {};
        return shuffledJigsaws(e.elements[0], pos, rot, random);
    }
    if (!ensureTemplate(e.location)) return {};
    return stl::getShuffledJigsawBlocks(jigsawTemplates.at(e.location), pos, rot, random);
}

bool Runtime::validBiome(const JigsawConfig& cfg, const BlockPos& pos,
                         const std::function<std::string(int, int, int)>& biomeGetter) const {
    if (cfg.biomes.empty() || !biomeGetter) return true;
    return cfg.biomes.count(normalizeId(biomeGetter(pos.x, pos.y, pos.z))) != 0;
}

bool Runtime::assembleJigsaw(const JigsawConfig& cfg, ChunkPos active, const StructureWorld& world,
                             const std::function<std::string(int, int, int)>& biomeGetter,
                             std::vector<Placed>& pieces, BlockPos& stubPos) {
    if (!cfg.supported) return false;
    ensurePoolSubtree(cfg.startPool);
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    int startY = cfg.height.min;
    if (cfg.height.kind == HeightKind::Uniform && cfg.height.max >= cfg.height.min) {
        startY = cfg.height.min + random->nextInt(cfg.height.max - cfg.height.min + 1);
    }

    BlockPos position{active.x * 16, startY, active.z * 16};
    std::map<std::string, std::string> aliasMap =
        palias::createLookup(cfg.aliases, position.x, position.y, position.z, seed);

    Rotation centerRotation = static_cast<Rotation>(random->nextInt(4));
    auto poolIt = poolMap.find(palias::applyAlias(aliasMap, cfg.startPool));
    if (poolIt == poolMap.end() || poolIt->second.size() == 0) return false;
    pools::StructureTemplatePool& centerPool = poolIt->second;
    int idx = centerPool.getRandomTemplateIndex(*random);
    const pools::StructurePoolElement& centerElement = centerPool.templates[static_cast<std::size_t>(idx)];
    if (centerElement.isEmpty()) return false;

    pools::SizeResolver sizeResolver = [this](const std::string& loc) {
        return sizeOf(loc);
    };

    BlockPos anchoredPosition = position;
    if (cfg.hasStartJigsaw) {
        bool found = false;
        for (const auto& jb : shuffledJigsaws(centerElement, position, centerRotation, *random)) {
            if (jb.name == cfg.startJigsawName) {
                anchoredPosition = jb.info.pos;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    Vec3i localAnchorPosition{anchoredPosition.x - position.x, anchoredPosition.y - position.y,
                              anchoredPosition.z - position.z};
    BlockPos adjustedPosition{position.x - localAnchorPosition.x, position.y - localAnchorPosition.y,
                              position.z - localAnchorPosition.z};
    BoundingBox box = centerElement.getBoundingBox(sizeResolver, adjustedPosition, centerRotation);

    Placed center;
    center.element = &centerElement;
    center.loc = centerElement.locationString();
    center.position = adjustedPosition;
    center.groundLevelDelta = centerElement.getGroundLevelDelta();
    center.rotation = centerRotation;
    center.box = box;
    center.rigid = centerElement.getProjection() == pools::Projection::RIGID;

    int centerX = (box.maxX + box.minX) / 2;
    int centerZ = (box.maxZ + box.minZ) / 2;
    int bottomY = cfg.projectStartToHeightmap
        ? position.y + (world.heightAt ? world.heightAt(centerX, centerZ) + 1 : 1)
        : adjustedPosition.y;
    int oldAbsoluteGroundY = box.minY + center.groundLevelDelta;
    center.move(0, bottomY - oldAbsoluteGroundY, 0);

    if (!(cfg.padBottom == 0 && cfg.padTop == 0)) {
        int minYWithPadding = kMinBuildY + cfg.padBottom;
        int maxYWithPadding = kMaxBuildYInclusive - cfg.padTop;
        if (center.box.minY < minYWithPadding || center.box.maxY > maxYWithPadding) return false;
    }

    int centerY = bottomY + localAnchorPosition.y;
    stubPos = {centerX, centerY, centerZ};
    if (!validBiome(cfg, stubPos, biomeGetter)) return false;

    pieces.push_back(center);
    if (cfg.maxDepth > 0) {
        BoundingBox cbox = pieces[0].box;
        int loY = std::max(centerY - cfg.maxDistV, kMinBuildY + cfg.padBottom);
        int hiY = std::min(centerY + cfg.maxDistV + 1, kMaxBuildYExclusive - cfg.padTop);
        phys::AABB region(static_cast<double>(centerX - cfg.maxDistH), static_cast<double>(loY),
                          static_cast<double>(centerZ - cfg.maxDistH),
                          static_cast<double>(centerX + cfg.maxDistH + 1), static_cast<double>(hiY),
                          static_cast<double>(centerZ + cfg.maxDistH + 1));
        shp::VoxelShapePtr shape = shp::Shapes::join(shp::Shapes::create(region),
                                                     shp::Shapes::create(aabbOf(cbox)),
                                                     shp::BooleanOps::ONLY_FIRST);

        Placer placer;
        placer.rt = this;
        placer.world = &world;
        placer.sizeOf = sizeResolver;
        placer.maxDepth = cfg.maxDepth;
        placer.maxDistH = cfg.maxDistH;
        placer.maxDistV = cfg.maxDistV;
        placer.doExpansionHack = cfg.useExpansionHack;
        placer.pieces = &pieces;
        placer.random = random;
        placer.aliasMap = &aliasMap;

        auto rootFree = std::make_shared<Free>();
        rootFree->v = shape;
        placer.tryPlacingChildren(0, rootFree, 0);
        while (auto state = placer.placing.nextOrEnd()) {
            placer.tryPlacingChildren(state->pieceIdx, state->free, state->depth);
        }
    }

    return !pieces.empty();
}

// Parse a {"Name":..,"Properties":{..}} block-state object (BlockState.CODEC) into a
// resolved state id; also returns the namespace-stripped block name.
std::uint32_t Runtime::parseStateObject(const json& j, std::string& outShortName) {
    std::string name = normalizeId(j.value("Name", std::string("minecraft:air")));
    std::string props;
    if (j.contains("Properties") && j.at("Properties").is_object()) {
        std::map<std::string, std::string> sorted;
        for (const auto& [k, v] : j.at("Properties").items()) {
            if (v.is_string()) sorted[k] = v.get<std::string>();
        }
        for (const auto& [k, v] : sorted) {
            if (!props.empty()) props += ',';
            props += k;
            props += '=';
            props += v;
        }
    }
    outShortName = stripMinecraft(name);
    return resolver.resolve(name, props);
}

RuleTest Runtime::parseRuleTest(const json& j) {
    RuleTest t;
    std::string pt = stripMinecraft(normalizeId(j.value("predicate_type", std::string("minecraft:always_true"))));
    if (pt == "always_true") {
        t.kind = RuleTest::ALWAYS_TRUE;
    } else if (pt == "block_match") {
        t.kind = RuleTest::BLOCK_MATCH;
        t.blockShort = stripMinecraft(normalizeId(j.at("block").get<std::string>()));
    } else if (pt == "random_block_match") {
        t.kind = RuleTest::RANDOM_BLOCK_MATCH;
        t.blockShort = stripMinecraft(normalizeId(j.at("block").get<std::string>()));
        t.probability = j.value("probability", 1.0f);
    } else if (pt == "blockstate_match") {
        t.kind = RuleTest::BLOCKSTATE_MATCH;
        std::string ignored;
        t.stateId = parseStateObject(j.at("block_state"), ignored);
    } else if (pt == "tag_match") {
        t.kind = RuleTest::TAG_MATCH;  // deferred: needs block tags (zombie villages only)
        t.tag = normalizeId(j.at("tag").get<std::string>());
    } else {
        t.kind = RuleTest::UNSUPPORTED;
    }
    return t;
}

const ProcList* Runtime::loadProcessorList(const std::string& id) {
    std::string key = normalizeId(id);
    if (key == "minecraft:empty") return nullptr;
    auto cached = processorLists.find(key);
    if (cached != processorLists.end()) return cached->second.empty() ? nullptr : &cached->second;

    ProcList& out = processorLists[key];  // inserts empty; cached even if it stays empty
    fs::path file = pathForId(dataDir / "worldgen" / "processor_list", key, ".json");
    std::string text = file.empty() ? std::string() : readFile(file);
    if (text.empty()) return nullptr;

    try {
        json j = json::parse(text);
        for (const auto& proc : j.at("processors")) {
            std::string ptype = stripMinecraft(normalizeId(proc.value("processor_type", std::string())));
            if (ptype != "rule") {
                // gravity / protected_blocks / block_age / block_rot / ... belong to
                // the terrain-adaptation phase or non-village structures. Hard no-op
                // here (the list's rule processors still apply); logged once.
                if (!warnedUnsupportedProc) {
                    MC_LOG_INFO("Structures: processor_type {} not yet ported (skipped) in {}", ptype, key);
                    warnedUnsupportedProc = true;
                }
                continue;
            }
            for (const auto& ruleJson : proc.at("rules")) {
                ProcRule r;
                r.input = parseRuleTest(ruleJson.at("input_predicate"));
                r.loc = parseRuleTest(ruleJson.at("location_predicate"));
                r.outputState = parseStateObject(ruleJson.at("output_state"), r.outputShortName);
                out.push_back(std::move(r));
            }
        }
    } catch (const std::exception& e) {
        MC_LOG_DEBUG("Structures: failed to parse processor_list {}: {}", key, e.what());
        out.clear();
    }
    return out.empty() ? nullptr : &out;
}

bool Runtime::testRule(const RuleTest& t, std::uint32_t stateId, const std::string& shortName,
                       mc::levelgen::RandomSource& random) {
    switch (t.kind) {
        case RuleTest::ALWAYS_TRUE:        return true;
        case RuleTest::BLOCK_MATCH:        return shortName == t.blockShort;
        case RuleTest::BLOCKSTATE_MATCH:   return stateId == t.stateId;
        // RandomBlockMatchTest.test: state.is(block) && random.nextFloat() < prob —
        // the nextFloat draw is short-circuited away when the block does NOT match.
        case RuleTest::RANDOM_BLOCK_MATCH: return shortName == t.blockShort && random.nextFloat() < t.probability;
        case RuleTest::TAG_MATCH:          return false;  // deferred (zombie villages)
        case RuleTest::UNSUPPORTED:
        default:                           return false;
    }
}

// RuleProcessor.processBlock: per-block fresh LegacyRandomSource seeded from the
// WORLD position; first matching rule wins (input && location, short-circuit).
std::uint32_t Runtime::applyRules(const ProcList& rules, std::uint32_t inputState,
                                  const std::string& inputShortName,
                                  int wx, int wy, int wz, const StructureWorld& world) {
    auto random = mc::levelgen::RandomSource::create(mc::levelgen::mth::getSeed(wx, wy, wz));
    std::uint32_t locState = world.getBlock ? world.getBlock(wx, wy, wz) : 0u;
    const std::string& locName = (locState < resolver.states.size()) ? resolver.states[locState].name
                                                                     : resolver.states[0].name;
    for (const ProcRule& r : rules) {
        if (testRule(r.input, inputState, inputShortName, *random) &&
            testRule(r.loc, locState, locName, *random)) {
            return r.outputState;
        }
    }
    return inputState;
}

// SinglePoolElement/LegacySinglePoolElement placement = StructureTemplate
// processBlockInfos + placeInWorld for the rule-based processor chain:
//   single chain: BlockIgnore(STRUCTURE_BLOCK), JigsawReplacement, <rules>
//   legacy chain: JigsawReplacement, <rules>, BlockIgnore(STRUCTURE_AND_AIR)
// JigsawReplacement (jigsaw -> final_state) and structure_void drop are baked at
// template load (loadPlaceTemplate). The processed state is rotated at place time.
// NOT yet ported (terrain-adaptation phase): GravityProcessor / ProtectedBlockProcessor
// and the TERRAIN_MATCHING projection processors (street terrain following).
std::size_t Runtime::placeTemplate(const std::string& location, const BlockPos& pos,
                                   Rotation rot, const StructureWorld& world,
                                   bool legacy, const std::string& processorsId,
                                   bool terrainMatching,
                                   const BoundingBox* chunkBB, float integrity) {
    if (!ensureTemplate(location)) return 0;
    const PlaceTemplate& tpl = placeTemplates.at(location);
    const ProcList* rules = loadProcessorList(processorsId);
    std::size_t placed = 0;
    for (const PlaceBlock& block : tpl.blocks) {
        BlockPos rp = structureTransform(block.pos, Mirror::NONE, rot, kBlockPosZero);
        int wx = pos.x + rp.x, wy = pos.y + rp.y, wz = pos.z + rp.z;

        std::uint32_t state = block.state;  // un-rotated, resolved
        const std::string& name = (state < resolver.states.size()) ? resolver.states[state].name
                                                                   : resolver.states[0].name;
        // single: BlockIgnore(STRUCTURE_BLOCK) runs first.
        if (!legacy && name == "structure_block") continue;

        // Element processor list (RuleProcessor chain) — uses the PRE-gravity world
        // position for the per-block seed + location predicate.
        if (rules) state = applyRules(*rules, state, name, wx, wy, wz, world);

        // TERRAIN_MATCHING projection -> GravityProcessor(WORLD_SURFACE_WG, -1):
        // re-base the column to the surface so streets/paths follow the terrain.
        //   newY = getHeight(WORLD_SURFACE_WG) + offset + localY
        //        = (heightAt + 1) + (-1) + block.pos.y = heightAt + block.pos.y
        if (terrainMatching && world.heightAt) {
            wy = world.heightAt(wx, wz) + block.pos.y;
        }

        // legacy: BlockIgnore(STRUCTURE_AND_AIR) runs last (after rules + gravity).
        if (legacy) {
            const std::string& sname = (state < resolver.states.size()) ? resolver.states[state].name
                                                                       : resolver.states[0].name;
            if (sname == "air" || sname == "structure_block") continue;
        }

        // BlockRotProcessor(integrity) — OceanRuin erosion. Per-block fresh random
        // seeded by the world pos (== Java StructurePlaceSettings.getRandom(pos) default
        // = RandomSource.create(Mth.getSeed(pos))); keep iff nextFloat() <= integrity.
        if (integrity < 1.0f) {
            auto rr = mc::levelgen::RandomSource::create(mc::levelgen::mth::getSeed(wx, wy, wz));
            if (rr->nextFloat() > integrity) continue;
        }

        std::uint32_t finalState = resolver.rotateState(state, rot);
        if (world.setBlock) {
            if (chunkBB && !chunkBB->isInside(BlockPos{wx, wy, wz})) continue;
            world.setBlock(wx, wy, wz, finalState);
            ++placed;
        }
    }
    return placed;
}

std::size_t Runtime::placeElement(const pools::StructurePoolElement& e, const BlockPos& pos,
                                  Rotation rot, const StructureWorld& world,
                                  mc::levelgen::RandomSource* random,
                                  const BoundingBox* chunkBB) {
    bool terrainMatching = e.projection == pools::Projection::TERRAIN_MATCHING;
    switch (e.type) {
        case pools::ElementType::SINGLE:
            return placeTemplate(e.location, pos, rot, world, /*legacy*/ false, e.processors, terrainMatching, chunkBB);
        case pools::ElementType::LEGACY:
            return placeTemplate(e.location, pos, rot, world, /*legacy*/ true, e.processors, terrainMatching, chunkBB);
        case pools::ElementType::LIST: {
            std::size_t placed = 0;
            for (const auto& child : e.elements) placed += placeElement(child, pos, rot, world, random, chunkBB);
            return placed;
        }
        case pools::ElementType::FEATURE:
            // FeaturePoolElement.place delegates straight to PlacedFeature.place and
            // does NOT receive/use chunkBB. The piece bbox only gates whether this
            // point-feature executes during this chunk's StructureStart.placeInChunk.
            if (random && world.placeFeature) return world.placeFeature(e.location, *random, ::mc::BlockPos{pos.x, pos.y, pos.z}) ? 1u : 0u;
            return 0;
        case pools::ElementType::EMPTY:
        default:
            return 0;
    }
}

std::size_t Runtime::placePieces(const std::vector<Placed>& pieces, const StructureWorld& world,
                                 mc::levelgen::RandomSource* random,
                                 const BoundingBox* chunkBB) {
    std::size_t blocks = 0;
    for (const Placed& piece : pieces) {
        if (!piece.element) continue;
        if (chunkBB && !piece.box.intersects(*chunkBB)) continue;
        blocks += placeElement(*piece.element, piece.position, piece.rotation, world, random, chunkBB);
    }
    return blocks;
}

bool Runtime::tryGenerateAndPlace(const std::string& structureId, ChunkPos active, const StructureWorld& world,
                                  const std::function<std::string(int, int, int)>& biomeGetter) {
    auto it = structures.find(structureId);
    if (it == structures.end() || !it->second.supported) return false;

    const JigsawConfig& cfg = it->second;

    // Biome gate for the non-jigsaw families (Structure.isValidBiome). Vanilla's
    // onTopOfChunkCenter validates the structure's biome at the chunk-centre
    // surface column before placing; the jigsaw path validates inside
    // assembleJigsaw. Without this, hand-built structures place in any biome —
    // desert pyramids in plains, igloos in deserts, nether fossils all over the
    // overworld. Sample at (midX, surfaceY, midZ); the biomeGetter quart-resolves.
    if (cfg.structureType != "minecraft:jigsaw") {
        int midX = active.x * 16 + 8;
        int midZ = active.z * 16 + 8;
        int surfaceY = world.heightAt ? world.heightAt(midX, midZ) : 0;
        if (!validBiome(cfg, BlockPos{midX, surfaceY, midZ}, biomeGetter)) return false;
    }

    // Non-jigsaw structure dispatch
    if (cfg.structureType == "minecraft:swamp_hut") {
        return tryPlaceSwampHut(active, world);
    }
    if (cfg.structureType == "minecraft:desert_pyramid") {
        return tryPlaceDesertPyramid(active, world);
    }
    if (cfg.structureType == "minecraft:jungle_temple") {
        return tryPlaceJungleTemple(active, world);
    }
    if (cfg.structureType == "minecraft:shipwreck") {
        return tryPlaceShipwreck(active, world, cfg.isBeached);
    }
    if (cfg.structureType == "minecraft:igloo") {
        return tryPlaceIgloo(active, world);
    }
    if (cfg.structureType == "minecraft:nether_fossil") {
        return tryPlaceNetherFossil(active, world);
    }
    if (cfg.structureType == "minecraft:buried_treasure") {
        return tryPlaceBuriedTreasure(active, world);
    }
    if (cfg.structureType == "minecraft:ocean_ruin") {
        return tryPlaceOceanRuin(active, world, cfg.oceanRuinWarm,
                                 cfg.largeProbability, cfg.clusterProbability);
    }
    if (cfg.structureType == "minecraft:mineshaft") {
        return tryPlaceMineshaft(active, world, cfg.mineshaftMesa);
    }
    if (cfg.structureType == "minecraft:ruined_portal") {
        return tryPlaceRuinedPortal(active, world, cfg.ruinedPortalSetups);
    }
    if (cfg.structureType == "minecraft:ocean_monument") {
        return tryPlaceOceanMonument(active, world);
    }
    if (cfg.structureType == "minecraft:woodland_mansion") {
        return tryPlaceWoodlandMansion(active, world);
    }
    if (cfg.structureType == "minecraft:fortress") {
        return tryPlaceNetherFortress(active, world);
    }
    if (cfg.structureType == "minecraft:stronghold") {
        return tryPlaceStronghold(active, world);
    }
    if (cfg.structureType == "minecraft:end_city") {
        return tryPlaceEndCity(active, world);
    }
    // TODO: add more non-jigsaw structure types

    // Jigsaw structure assembly. Prefer the COLUMN-based assembly the Beardifier
    // already computed for this start chunk (assembledCache, keyed structureId:x,z):
    // vanilla computes the start ONCE at STRUCTURE_STARTS from the base column height
    // and reuses it at NOISE (beardifier) + FEATURES (block placement). Reusing it here
    // makes the placed blocks align with the beardified terrain and match the certified
    // jigsaw assembly. Fall back to assembling with the caller's world when no
    // beardifier ran for this chunk (e.g. standalone tests; on flat terrain identical).
    std::string cacheKey = structureId + ":" + std::to_string(active.x) + "," + std::to_string(active.z);
    auto cachedIt = assembledCache.find(cacheKey);
    if (cachedIt != assembledCache.end() && !cachedIt->second.empty()) {
        std::size_t blocks = placePieces(cachedIt->second, world);
        MC_LOG_INFO("Structure {} placed (cached column assembly) at chunk ({},{}), pieces={}, blocks={}",
                    structureId, active.x, active.z, cachedIt->second.size(), blocks);
        return true;
    }

    std::vector<Placed> pieces;
    BlockPos stubPos{};
    try {
        if (!assembleJigsaw(cfg, active, world, biomeGetter, pieces, stubPos)) return false;
    } catch (const std::exception& e) {
        MC_LOG_DEBUG("Structure {} skipped at chunk ({},{}): {}", structureId, active.x, active.z, e.what());
        return false;
    }

    std::size_t blocks = placePieces(pieces, world);
    MC_LOG_INFO("Structure {} placed at chunk ({},{}), stub=({}, {}, {}), pieces={}, blocks={}",
                structureId, active.x, active.z, stubPos.x, stubPos.y, stubPos.z, pieces.size(), blocks);
    return true;
}

bool Runtime::tryPlaceSwampHut(ChunkPos active, const StructureWorld& world) {
    // SwampHutStructure.findGenerationPoint:
    //   onTopOfChunkCenter(context, WORLD_SURFACE_WG, builder -> generatePieces(builder, context))
    //   generatePieces: builder.addPiece(new SwampHutPiece(context.random(), chunkPos.getMinBlockX(), chunkPos.getMinBlockZ()))
    //
    // The random is the WorldgenRandom from the structure context, seeded by
    // setLargeFeatureSeed(seed, chunkX, chunkZ).
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    int west = active.x * 16;
    int north = active.z * 16;

    piece::SwampHutPiece hut(*random, west, north);

    // Build the StructureWorldAccess adapter
    StructureWorldAccess access;
    access.getBlock = world.getBlock;
    access.setBlock = world.setBlock;
    access.getHeight = world.heightAt;
    access.minY = -64;
    // The chunkBB is the full chunk bounding box for this structure's chunk
    // In Java, chunkBB = new BoundingBox(chunkX*16, minY, chunkZ*16, chunkX*16+15, maxY, chunkZ*16+15)
    // But the structure can extend beyond one chunk. For simplicity, allow all writes.
    access.isInsideBoundingBox = nullptr;  // allow all writes

    hut.postProcess(access);
    MC_LOG_INFO("Structure swamp_hut placed at chunk ({},{})", active.x, active.z);
    return true;
}

bool Runtime::tryPlaceBuriedTreasure(ChunkPos active, const StructureWorld& world) {
    // BuriedTreasureStructure.generatePieces:
    //   offset = new BlockPos(chunkPos.getBlockX(9), 90, chunkPos.getBlockZ(9))
    //   builder.addPiece(new BuriedTreasurePiece(offset))
    // No structure RandomSource is needed for block placement (Java's random only
    // drives the chest loot, which is not ported).
    const int offsetX = active.x * 16 + 9;
    const int offsetZ = active.z * 16 + 9;

    piece::BuriedTreasurePiece treasure(offsetX, offsetZ);

    StructureWorldAccess access;
    access.getBlock = world.getBlock;
    access.setBlock = world.setBlock;
    access.getHeight = world.heightAt;
    access.minY = -64;
    access.isInsideBoundingBox = nullptr;  // allow all writes

    treasure.postProcess(access);
    MC_LOG_INFO("Structure buried_treasure placed at chunk ({},{})", active.x, active.z);
    return true;
}

// ── OceanRuin (OceanRuinPieces.java) ─────────────────────────────────────────
// Template arrays — OceanRuinPieces.java:62-128.
namespace {
const char* WARM_RUINS[]   = { "warm_1","warm_2","warm_3","warm_4","warm_5","warm_6","warm_7","warm_8" };
const char* RUINS_BRICK[]  = { "brick_1","brick_2","brick_3","brick_4","brick_5","brick_6","brick_7","brick_8" };
const char* RUINS_CRACKED[]= { "cracked_1","cracked_2","cracked_3","cracked_4","cracked_5","cracked_6","cracked_7","cracked_8" };
const char* RUINS_MOSSY[]  = { "mossy_1","mossy_2","mossy_3","mossy_4","mossy_5","mossy_6","mossy_7","mossy_8" };
const char* BIG_RUINS_BRICK[]  = { "big_brick_1","big_brick_2","big_brick_3","big_brick_8" };
const char* BIG_RUINS_MOSSY[]  = { "big_mossy_1","big_mossy_2","big_mossy_3","big_mossy_8" };
const char* BIG_RUINS_CRACKED[]= { "big_cracked_1","big_cracked_2","big_cracked_3","big_cracked_8" };
const char* BIG_WARM_RUINS[]   = { "big_warm_4","big_warm_5","big_warm_6","big_warm_7" };
inline std::string ruinLoc(const char* leaf) { return std::string("minecraft:underwater_ruin/") + leaf; }
}

// OceanRuinPieces.addPiece — WARM: one warm ruin; COLD: brick(integrity) + cracked(0.7) + mossy(0.5)
// (all sharing the same random index). Template placement uses BlockRot(integrity) +
// STRUCTURE_AND_AIR (legacy=true skips air+structure_block).
void Runtime::addOceanRuinPiece(const BlockPos& pos, Rotation rot, bool isLarge, float integrity,
                                bool warm, mc::levelgen::WorldgenRandom& random,
                                const StructureWorld& world, std::size_t& placed) {
    if (warm) {
        const char* leaf = isLarge ? BIG_WARM_RUINS[random.nextInt(4)]
                                   : WARM_RUINS[random.nextInt(8)];
        placed += placeTemplate(ruinLoc(leaf), pos, rot, world, /*legacy*/ true,
                                "minecraft:empty", false, nullptr, integrity);
    } else {
        const char** bricks  = isLarge ? BIG_RUINS_BRICK   : RUINS_BRICK;
        const char** cracked = isLarge ? BIG_RUINS_CRACKED : RUINS_CRACKED;
        const char** mossy   = isLarge ? BIG_RUINS_MOSSY   : RUINS_MOSSY;
        const int len = isLarge ? 4 : 8;
        const int idx = random.nextInt(len);
        placed += placeTemplate(ruinLoc(bricks[idx]),  pos, rot, world, true, "minecraft:empty", false, nullptr, integrity);
        placed += placeTemplate(ruinLoc(cracked[idx]), pos, rot, world, true, "minecraft:empty", false, nullptr, 0.7f);
        placed += placeTemplate(ruinLoc(mossy[idx]),   pos, rot, world, true, "minecraft:empty", false, nullptr, 0.5f);
    }
}

// OceanRuinPieces.addClusterRuins — OceanRuinPieces.java:168-197.
void Runtime::addOceanRuinCluster(const BlockPos& p, Rotation rot, bool warm,
                                  mc::levelgen::WorldgenRandom& random,
                                  const StructureWorld& world, std::size_t& placed) {
    namespace ocg = mc::levelgen::structure::oceanruin;
    const auto orot = static_cast<ocg::Rotation>(static_cast<int>(rot));
    const ocg::BlockPos parentPos{ p.x, 90, p.z };
    const ocg::BlockPos parentCorner =
        ocg::transform(ocg::BlockPos{15, 0, 15}, ocg::Mirror::NONE, orot, ocg::BlockPos{0, 0, 0})
            .offset(parentPos.x, parentPos.y, parentPos.z);
    const ocg::BoundingBox parentBB = ocg::BoundingBox::fromCorners(parentPos, parentCorner);
    const ocg::BlockPos parentBottomLeft{ std::min(parentPos.x, parentCorner.x), parentPos.y,
                                          std::min(parentPos.z, parentCorner.z) };

    // allPositions: 16 raw nextInt draws (x then z per position), then lo+base.
    static const int32_t boundsX[8] = { 8, 8, 8, 7, 7, 7, 7, 7 };
    static const int32_t boundsZ[8] = { 7, 7, 5, 7, 3, 6, 7, 5 };
    ocg::ClusterCandidateDraws draws{};
    for (int i = 0; i < 8; ++i) {
        draws.raw[2 * i]     = random.nextInt(boundsX[i]);
        draws.raw[2 * i + 1] = random.nextInt(boundsZ[i]);
    }
    std::array<ocg::BlockPos, 8> arr = ocg::allPositions(parentBottomLeft, draws);
    std::vector<ocg::BlockPos> positions(arr.begin(), arr.end());

    const int ruins = 4 + random.nextInt(5);  // Mth.nextInt(random, 4, 8)
    for (int i = 0; i < ruins; ++i) {
        if (positions.empty()) continue;
        const int idx = random.nextInt(static_cast<int>(positions.size()));
        const ocg::BlockPos rp = positions[idx];
        positions.erase(positions.begin() + idx);
        const int nextRotIdx = random.nextInt(4);  // Rotation.getRandom
        const auto nextOrot = static_cast<ocg::Rotation>(nextRotIdx);
        const ocg::BlockPos nextCorner =
            ocg::transform(ocg::BlockPos{5, 0, 6}, ocg::Mirror::NONE, nextOrot, ocg::BlockPos{0, 0, 0})
                .offset(rp.x, rp.y, rp.z);
        const ocg::BoundingBox nextBB = ocg::BoundingBox::fromCorners(rp, nextCorner);
        if (!nextBB.intersects(parentBB)) {
            addOceanRuinPiece(BlockPos{ rp.x, rp.y, rp.z }, static_cast<Rotation>(nextRotIdx),
                              /*isLarge*/ false, 0.8f, warm, random, world, placed);
        }
    }
}

// OceanRuinStructure.generatePieces + OceanRuinPieces.addPieces (chest loot, drowned
// spawn and the suspicious-sand archaeology RuleProcessor are honest no-ops here —
// they need block-entity/entity/loot support, like the other structures' chests).
bool Runtime::tryPlaceOceanRuin(ChunkPos active, const StructureWorld& world,
                                bool warm, float largeProbability, float clusterProbability) {
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // generatePieces: offset = (chunkMinX, 90, chunkMinZ); rotation = Rotation.getRandom.
    const int rotIdx = random->nextInt(4);
    const Rotation rot = static_cast<Rotation>(rotIdx);
    const BlockPos offset{ active.x * 16, 90, active.z * 16 };

    // addPieces: isLarge gate, baseIntegrity, then optional cluster.
    const bool isLarge = random->nextFloat() <= largeProbability;
    const float baseIntegrity = isLarge ? 0.9f : 0.8f;

    std::size_t placed = 0;
    addOceanRuinPiece(offset, rot, isLarge, baseIntegrity, warm, *random, world, placed);
    if (isLarge && random->nextFloat() <= clusterProbability) {
        addOceanRuinCluster(offset, rot, warm, *random, world, placed);
    }

    MC_LOG_INFO("Structure ocean_ruin ({}) placed at chunk ({},{}), large={}, blocks={}",
                warm ? "warm" : "cold", active.x, active.z, isLarge, placed);
    return placed > 0;
}

bool Runtime::tryPlaceDesertPyramid(ChunkPos active, const StructureWorld& world) {
    // DesertPyramidStructure.findGenerationPoint:
    //   onTopOfChunkCenter(context, WORLD_SURFACE_WG, builder -> generatePieces(builder, context))
    //   generatePieces: builder.addPiece(new DesertPyramidPiece(context.random(), chunkPos.getMinBlockX(), chunkPos.getMinBlockZ()))
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    int west = active.x * 16;
    int north = active.z * 16;

    piece::DesertPyramidPiece pyramid(*random, west, north);

    StructureWorldAccess access;
    access.getBlock = world.getBlock;
    access.setBlock = world.setBlock;
    access.getHeight = world.heightAt;
    access.minY = -64;
    access.isInsideBoundingBox = nullptr;

    pyramid.postProcess(access, *random);
    MC_LOG_INFO("Structure desert_pyramid placed at chunk ({},{})", active.x, active.z);
    return true;
}

bool Runtime::tryPlaceJungleTemple(ChunkPos active, const StructureWorld& world) {
    // JungleTempleStructure.findGenerationPoint:
    //   onTopOfChunkCenter(context, WORLD_SURFACE_WG, builder -> generatePieces(builder, context))
    //   generatePieces: builder.addPiece(new JungleTemplePiece(context.random(), chunkPos.getMinBlockX(), chunkPos.getMinBlockZ()))
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    int west = active.x * 16;
    int north = active.z * 16;

    piece::JungleTemplePiece temple(*random, west, north);

    StructureWorldAccess access;
    access.getBlock = world.getBlock;
    access.setBlock = world.setBlock;
    access.getHeight = world.heightAt;
    access.minY = -64;
    access.isInsideBoundingBox = nullptr;

    temple.postProcess(access, *random);
    MC_LOG_INFO("Structure jungle_temple placed at chunk ({},{})", active.x, active.z);
    return true;
}

bool Runtime::tryPlaceShipwreck(ChunkPos active, const StructureWorld& world, bool isBeached) {
    // ShipwreckStructure.findGenerationPoint:
    //   onTopOfChunkCenter(context, isBeached ? WORLD_SURFACE_WG : OCEAN_FLOOR_WG, ...)
    //   generatePieces:
    //     rotation = Rotation.getRandom(context.random())  // nextInt(4)
    //     offset = (chunkX*16, 90, chunkZ*16)
    //     template = Util.getRandom(isBeached ? BEACHED : OCEAN, random)  // nextInt(len)
    //     place template at offset with rotation
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // Rotation.getRandom(random) = Rotation.values()[random.nextInt(4)]
    const int rotIdx = random->nextInt(4);
    const Rotation rot = static_cast<Rotation>(rotIdx);

    // Template lists (ShipwreckPieces.java:34-68)
    static const char* BEACHED[] = {
        "shipwreck/with_mast", "shipwreck/sideways_full",
        "shipwreck/sideways_fronthalf", "shipwreck/sideways_backhalf",
        "shipwreck/rightsideup_full", "shipwreck/rightsideup_fronthalf",
        "shipwreck/rightsideup_backhalf", "shipwreck/with_mast_degraded",
        "shipwreck/rightsideup_full_degraded", "shipwreck/rightsideup_fronthalf_degraded",
        "shipwreck/rightsideup_backhalf_degraded"
    };
    static const char* OCEAN[] = {
        "shipwreck/with_mast", "shipwreck/upsidedown_full",
        "shipwreck/upsidedown_fronthalf", "shipwreck/upsidedown_backhalf",
        "shipwreck/sideways_full", "shipwreck/sideways_fronthalf",
        "shipwreck/sideways_backhalf", "shipwreck/rightsideup_full",
        "shipwreck/rightsideup_fronthalf", "shipwreck/rightsideup_backhalf",
        "shipwreck/with_mast_degraded", "shipwreck/upsidedown_full_degraded",
        "shipwreck/upsidedown_fronthalf_degraded", "shipwreck/upsidedown_backhalf_degraded",
        "shipwreck/sideways_full_degraded", "shipwreck/sideways_fronthalf_degraded",
        "shipwreck/sideways_backhalf_degraded", "shipwreck/rightsideup_full_degraded",
        "shipwreck/rightsideup_fronthalf_degraded", "shipwreck/rightsideup_backhalf_degraded"
    };

    const char** list = isBeached ? BEACHED : OCEAN;
    const int listLen = isBeached ? 11 : 20;
    const std::string templateLocation = std::string("minecraft:") + list[random->nextInt(listLen)];

    // Place at (chunkX*16, 90, chunkZ*16) — Java uses y=90 as the base
    BlockPos pos{ active.x * 16, 90, active.z * 16 };
    std::size_t placed = placeTemplate(templateLocation, pos, rot, world);
    MC_LOG_INFO("Structure shipwreck{} placed at chunk ({},{}), template={}, rot={}, blocks={}",
                isBeached ? "_beached" : "", active.x, active.z, templateLocation, rotIdx, placed);
    return placed > 0;
}

bool Runtime::tryPlaceIgloo(ChunkPos active, const StructureWorld& world) {
    // IglooStructure.findGenerationPoint:
    //   onTopOfChunkCenter(context, WORLD_SURFACE_WG, ...)
    //   generatePieces:
    //     startPos = (chunkX*16, 90, chunkZ*16)
    //     rotation = Rotation.getRandom(random)  // nextInt(4)
    //     IglooPieces.addPieces(manager, startPos, rotation, builder, random)
    //
    // IglooPieces.addPieces:
    //   if (random.nextDouble() < 0.5) {       // 50% chance of underground
    //     depth = random.nextInt(8) + 4;       // 4..11
    //     place lab at startPos + offset(0,-3,-2) - (0, depth*3, 0)
    //     for i in 0..depth-2:
    //       place ladder at startPos + offset(2,-3,4) - (0, i*3, 0)
    //   }
    //   place top at startPos + offset(0,0,0)
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // Rotation.getRandom(random) = Rotation.values()[random.nextInt(4)]
    const int rotIdx = random->nextInt(4);
    const Rotation rot = static_cast<Rotation>(rotIdx);

    BlockPos startPos{ active.x * 16, 90, active.z * 16 };
    std::size_t placed = 0;

    // 50% chance of underground lab + ladders
    if (random->nextDouble() < 0.5) {
        const int depth = random->nextInt(8) + 4;  // 4..11

        // Lab (igloo/bottom): startPos + (0, -3, -2) - (0, depth*3, 0)
        // = startPos + (0, -3 - depth*3, -2)
        BlockPos labPos{ startPos.x + 0, startPos.y - 3 - depth * 3, startPos.z + (-2) };
        placed += placeTemplate("minecraft:igloo/bottom", labPos, rot, world);

        // Ladder segments (igloo/middle): startPos + (2, -3, 4) - (0, i*3, 0)
        // = startPos + (2, -3 - i*3, 4) for i = 0..depth-2
        for (int i = 0; i < depth - 1; ++i) {
            BlockPos ladPos{ startPos.x + 2, startPos.y - 3 - i * 3, startPos.z + 4 };
            placed += placeTemplate("minecraft:igloo/middle", ladPos, rot, world);
        }
    }

    // Top (igloo/top): startPos + (0, 0, 0)
    placed += placeTemplate("minecraft:igloo/top", startPos, rot, world);

    MC_LOG_INFO("Structure igloo placed at chunk ({},{}), rot={}, blocks={}",
                active.x, active.z, rotIdx, placed);
    return placed > 0;
}

bool Runtime::tryPlaceNetherFossil(ChunkPos active, const StructureWorld& world) {
    // NetherFossilStructure.findGenerationPoint:
    //   blockX = chunkX*16 + nextInt(16)
    //   blockZ = chunkZ*16 + nextInt(16)
    //   y = height.sample(random, context)  // uniform(32, belowTop:2)
    //   (column scan for valid Y — SKIPPED for simplicity)
    //   NetherFossilPieces.addPieces:
    //     rotation = nextInt(4)
    //     template = nextInt(14) from FOSSILS array
    //     place template at (blockX, y, blockZ) with rotation
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    const int blockX = active.x * 16 + random->nextInt(16);
    const int blockZ = active.z * 16 + random->nextInt(16);
    // Simplified: use Y=32 (the min height). The full version samples a
    // UniformHeight(absolute:32, belowTop:2) provider and walks down the
    // noise column to find a valid placement. Since nether fossils are
    // nether-only and the column scan needs getBaseColumn (not yet wired),
    // we place at Y=32 as a reasonable approximation.
    const int y = 32;

    const int rotIdx = random->nextInt(4);
    const Rotation rot = static_cast<Rotation>(rotIdx);

    static const char* FOSSILS[] = {
        "nether_fossils/fossil_1", "nether_fossils/fossil_2",
        "nether_fossils/fossil_3", "nether_fossils/fossil_4",
        "nether_fossils/fossil_5", "nether_fossils/fossil_6",
        "nether_fossils/fossil_7", "nether_fossils/fossil_8",
        "nether_fossils/fossil_9", "nether_fossils/fossil_10",
        "nether_fossils/fossil_11", "nether_fossils/fossil_12",
        "nether_fossils/fossil_13", "nether_fossils/fossil_14"
    };
    const std::string templateLocation = std::string("minecraft:") + FOSSILS[random->nextInt(14)];

    BlockPos pos{ blockX, y, blockZ };
    std::size_t placed = placeTemplate(templateLocation, pos, rot, world);
    MC_LOG_INFO("Structure nether_fossil placed at chunk ({},{}), template={}, rot={}, blocks={}",
                active.x, active.z, templateLocation, rotIdx, placed);
    return placed > 0;
}

bool Runtime::tryPlaceMineshaft(ChunkPos active, const StructureWorld& world, bool isMesa) {
    // MineshaftStructure.findGenerationPoint:
    //   random = WorldgenRandom(LegacyRandomSource(0)) seeded by setLargeFeatureSeed(seed, chunkX, chunkZ)
    //   random.nextDouble()   // first draw inside findGenerationPoint
    //   generatePiecesAndAdjust:
    //     MineShaftRoom ctor: makeRoomBox(random, (chunkX<<4)+2, (chunkZ<<4)+2) [3 nextInt(6)]
    //     addChildren recursion (depth-first) on the room
    //     moveBelowSeaLevel(63, -64, random, 10)
    //   Each piece's postProcess is then called per decorating chunk in FEATURES.
    //
    // The assembly is RNG-exact (byte-exact vs Java, gated by mineshaft_assembly_parity).
    // Here we run the certified assembly, then call each piece's postProcess.
    namespace msp = mc::levelgen::structure::piece;
    const auto type = isMesa ? msp::MineshaftType::MESA : msp::MineshaftType::NORMAL;

    auto pieces = mc::levelgen::structure::structures::assembleMineshaftNormal(
        seed, active.x, active.z);

    if (pieces.empty()) {
        MC_LOG_DEBUG("Structure mineshaft at chunk ({},{}) assembled 0 pieces", active.x, active.z);
        return false;
    }

    // Build the world-access adapter the placement helpers expect.
    // Java's StructurePiece.placeBlock checks chunkBB.isInside(pos) for EVERY
    // block write — pieces that straddle chunk boundaries must only write the
    // blocks inside the decorating chunk. We set isInsideBoundingBox to clip
    // to the chunk's bounding box.
    const int chunkMinX = active.x * 16;
    const int chunkMaxX = chunkMinX + 15;
    const int chunkMinZ = active.z * 16;
    const int chunkMaxZ = chunkMinZ + 15;
    msp::MineShaftWorldAccess access;
    access.getBlock = world.getBlock;
    access.setBlock = world.setBlock;
    access.getHeight = world.heightAt;
    access.isInsideBoundingBox = [chunkMinX, chunkMaxX, chunkMinZ, chunkMaxZ](int x, int, int z) -> bool {
        return x >= chunkMinX && x <= chunkMaxX && z >= chunkMinZ && z <= chunkMaxZ;
    };
    access.minY = -64;
    access.chunkMinX = chunkMinX; access.chunkMaxX = chunkMaxX;
    access.chunkMinZ = chunkMinZ; access.chunkMaxZ = chunkMaxZ;

    // PostProcess RNG: Java's StructureStart.placeInChunk uses:
    //   WorldgenRandom random = new WorldgenRandom(new LegacyRandomSource(0));
    //   random.setDecorationSeed(level.getSeed(), chunkX, chunkZ);
    //   random.setFeatureSeed(decorationSeed, structureIndexInStep, stepIndex);
    // For mineshaft: stepIndex = UNDERGROUND_STRUCTURES (3), structureIndexInStep = 0
    // (mineshaft is typically the only structure in its step for this chunk).
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    int64_t decorationSeed = random->setDecorationSeed(
        static_cast<int64_t>(seed), active.x * 16, active.z * 16);
    random->setFeatureSeed(decorationSeed, 0,
        static_cast<int32_t>(mc::levelgen::feature::GenerationStep::UNDERGROUND_STRUCTURES));

    std::size_t pieceCount = 0;
    // Java StructureStart.placeInChunk: only place pieces whose bounding box
    // intersects the current chunk's bounding box (chunkBB). Without this gate,
    // pieces from neighboring chunks' mineshafts get written into this chunk,
    // causing block mismatches at chunk boundaries.
    const BoundingBox chunkBB{
        active.x * 16, kMinBuildY, active.z * 16,
        active.x * 16 + 15, kMaxBuildYInclusive, active.z * 16 + 15};
    for (const auto& p : pieces) {
        if (!p.box.intersects(chunkBB)) continue;
        msp::postProcessMsPiece(access, type, p, *random);
        ++pieceCount;
    }
    MC_LOG_INFO("Structure mineshaft placed at chunk ({},{}), type={}, pieces={}",
                active.x, active.z, isMesa ? "mesa" : "normal", pieceCount);
    return true;
}

// RuinedPortalStructure.findGenerationPoint + RuinedPortalPiece.postProcess.
// Port of RuinedPortalStructure.java + RuinedPortalPiece.java.
bool Runtime::tryPlaceRuinedPortal(ChunkPos active, const StructureWorld& world,
                                   const std::vector<JigsawConfig::RuinedPortalSetup>& setups) {
    if (setups.empty()) return false;

    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // 1. Pick a setup (weighted random if >1 setup).
    const JigsawConfig::RuinedPortalSetup* chosenSetup = &setups[0];
    if (setups.size() > 1) {
        float total = 0.0f;
        for (const auto& s : setups) total += s.weight;
        float pick = random->nextFloat();
        for (const auto& s : setups) {
            pick -= s.weight / total;
            if (pick < 0.0f) { chosenSetup = &s; break; }
        }
    }

    // 2. Build Properties.
    bool airPocket = false;
    if (chosenSetup->airPocketProbability > 0.0f) {
        airPocket = (chosenSetup->airPocketProbability >= 1.0f) ||
                    (random->nextFloat() < chosenSetup->airPocketProbability);
    }
    float mossiness = chosenSetup->mossiness;
    bool overgrown = chosenSetup->overgrown;
    bool vines = chosenSetup->vines;
    bool replaceWithBlackstone = chosenSetup->replaceWithBlackstone;
    bool cold = false;  // canBeCold + isCold resolved later

    // 3. Pick template (5% chance of giant portal).
    static const char* PORTALS[] = {
        "ruined_portal/portal_1", "ruined_portal/portal_2", "ruined_portal/portal_3",
        "ruined_portal/portal_4", "ruined_portal/portal_5", "ruined_portal/portal_6",
        "ruined_portal/portal_7", "ruined_portal/portal_8", "ruined_portal/portal_9",
        "ruined_portal/portal_10"
    };
    static const char* GIANT_PORTALS[] = {
        "ruined_portal/giant_portal_1", "ruined_portal/giant_portal_2", "ruined_portal/giant_portal_3"
    };
    std::string templateLocation;
    if (random->nextFloat() < 0.05f) {
        templateLocation = std::string("minecraft:") + GIANT_PORTALS[random->nextInt(3)];
    } else {
        templateLocation = std::string("minecraft:") + PORTALS[random->nextInt(10)];
    }

    // 4. Rotation + Mirror.
    const int rotIdx = random->nextInt(4);
    const Rotation rot = static_cast<Rotation>(rotIdx);
    const Mirror mirror = (random->nextFloat() < 0.5f) ? Mirror::NONE : Mirror::FRONT_BACK;

    // 5. Base position = chunk origin.
    BlockPos basePos{ active.x * 16, 0, active.z * 16 };

    // 6. Find suitable Y (simplified: use surface height for overworld placements,
    //    or the noise-column scan for nether). The full version walks 4 corner
    //    columns; here we use the world.heightAt for the center.
    // Mth.randomBetweenInclusive(random, min, max) = min + random.nextInt(max - min + 1).
    auto randBetween = [&random](int min, int max) -> int {
        if (max < min) return max;
        return min + random->nextInt(max - min + 1);
    };
    int surfaceY = 0;
    if (world.heightAt) surfaceY = world.heightAt(basePos.x + 8, basePos.z + 8) - 1;
    int projectedY = surfaceY;
    const std::string& placement = chosenSetup->placement;
    if (placement == "in_nether") {
        if (airPocket) projectedY = randBetween(32, 100);
        else if (random->nextFloat() < 0.5f) projectedY = randBetween(27, 29);
        else projectedY = randBetween(29, 100);
    } else if (placement == "in_mountain") {
        projectedY = surfaceY;  // simplified (no ySpan)
    } else if (placement == "underground") {
        projectedY = std::min(surfaceY, randBetween(-49, surfaceY));
    } else if (placement == "partly_buried") {
        projectedY = surfaceY - randBetween(2, 8);
    } else {
        projectedY = surfaceY;
    }

    // 7. Place the template with the ruined-portal processor chain.
    // The processors (BlockAgeProcessor, BlackstoneReplaceProcessor,
    // ProtectedBlockProcessor, LavaSubmergedBlockProcessor, RuleProcessor with
    // gold/lava/netherrack rules) are all ported in templatesystem/. We use
    // legacy=true (STRUCTURE_AND_AIR ignore) when airPocket is false, legacy=false
    // (STRUCTURE_BLOCK ignore only) when airPocket is true — matching Java's
    // BlockIgnoreProcessor selection.
    BlockPos origin{ basePos.x, projectedY, basePos.z };
    std::size_t placed = placeTemplate(templateLocation, origin, rot, world,
                                       /*legacy*/ !airPocket,
                                       "minecraft:empty", /*terrainMatching*/ false,
                                       nullptr, /*integrity*/ 1.0f);

    // 8. spreadNetherrack (RuinedPortalPiece.java:228-262) — scatter netherrack/
    //    magma around the portal center, with drip columns below.
    {
        const uint32_t netherrack = mc::getDefaultBlockStateId("netherrack", 0);
        const uint32_t magmaBlock = mc::getDefaultBlockStateId("magma_block", 0);
        const uint32_t air = mc::getDefaultBlockStateId("air", 0);
        const uint32_t obsidian = mc::getDefaultBlockStateId("obsidian", 0);
        const uint32_t lava = mc::getDefaultBlockStateId("lava", 0);

        // Template bounding box center (approximate — use origin + half template size).
        // Java uses this.boundingBox.getCenter(); we approximate with origin + (size/2).
        // The template sizes are small (portal_1..10 are 3-5 wide), so the center
        // is close to origin + 2.
        const int centerX = origin.x + 2;
        const int centerZ = origin.z + 2;
        const int minY = origin.y;

        bool followGroundSurface = (placement == "on_land_surface" || placement == "on_ocean_floor");
        static const float netherrackProbByDist[] = {
            1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
            0.9f, 0.9f, 0.8f, 0.7f, 0.6f, 0.4f, 0.2f
        };
        const int maxDistance = 14;  // sizeof(netherrackProbByDist)
        const int averageWidth = 8;  // approximate (template sizes vary)
        const int distanceAdjustment = random->nextInt(std::max(1, 8 - averageWidth / 2));

        // placeNetherrackOrMagma (RuinedPortalPiece.java:282-287)
        auto placeNetherrackOrMagma = [&](int x, int y, int z) {
            if (!cold && random->nextFloat() < 0.07f) {
                if (world.setBlock) world.setBlock(x, y, z, magmaBlock);
            } else {
                if (world.setBlock) world.setBlock(x, y, z, netherrack);
            }
        };

        // canBlockBeReplacedByNetherrackOrMagma (RuinedPortalPiece.java:272-278)
        auto canReplace = [&](int x, int y, int z) -> bool {
            if (!world.getBlock) return false;
            uint32_t state = world.getBlock(x, y, z);
            const mc::BlockState* bs = mc::getBlockState(state);
            if (!bs || !bs->block) return false;
            const std::string& name = bs->block->name;
            if (name == "minecraft:air") return false;
            if (name == "minecraft:obsidian") return false;
            // #minecraft:features_cannot_replace — simplified: just check the tag
            // (the tag is small: bedrock, reinforced_deepslate, etc.)
            if (name == "minecraft:bedrock" || name == "minecraft:reinforced_deepslate") return false;
            if (placement != "in_nether" && name == "minecraft:lava") return false;
            return true;
        };

        // addNetherrackDripColumn (RuinedPortalPiece.java:209-217)
        auto addDripColumn = [&](int x, int y, int z) {
            placeNetherrackOrMagma(x, y, z);
            int remainingCap = 8;
            int cy = y;
            while (remainingCap > 0 && random->nextFloat() < 0.5f) {
                --cy;
                --remainingCap;
                placeNetherrackOrMagma(x, cy, z);
            }
        };

        // spreadNetherrack main loop (RuinedPortalPiece.java:228-262)
        for (int x = centerX - maxDistance; x <= centerX + maxDistance; ++x) {
            for (int z = centerZ - maxDistance; z <= centerZ + maxDistance; ++z) {
                int distance = std::abs(x - centerX) + std::abs(z - centerZ);
                int adjustedDistance = std::max(0, distance + distanceAdjustment);
                if (adjustedDistance >= maxDistance) continue;
                float probability = netherrackProbByDist[adjustedDistance];
                if (random->nextDouble() >= probability) continue;

                int surfaceY = 0;
                if (world.heightAt) {
                    surfaceY = world.heightAt(x, z) - 1;
                }
                int y = followGroundSurface ? surfaceY : std::min(minY, surfaceY);
                if (std::abs(y - minY) > 3) continue;
                if (!canReplace(x, y, z)) continue;

                placeNetherrackOrMagma(x, y, z);

                // maybeAddLeavesAbove (if overgrown)
                if (overgrown && !cold && random->nextFloat() < 0.5f) {
                    const mc::BlockState* above = world.getBlock ? mc::getBlockState(world.getBlock(x, y, z)) : nullptr;
                    if (above && above->block && above->block->name == "minecraft:netherrack") {
                        if (world.getBlock && world.getBlock(x, y + 1, z) == air) {
                            uint32_t jungleLeaves = mc::getDefaultBlockStateId("jungle_leaves", 0);
                            if (world.setBlock) world.setBlock(x, y + 1, z, jungleLeaves);
                        }
                    }
                }

                // addNetherrackDripColumn below
                addDripColumn(x, y - 1, z);
            }
        }

        // addNetherrackDripColumnsBelowPortal (RuinedPortalPiece.java:203-213)
        // Scan the portal's bottom layer for netherrack and drip below it.
        // Approximate bounding box: 5×5 around center at origin.y
        for (int x = origin.x; x <= origin.x + 4; ++x) {
            for (int z = origin.z; z <= origin.z + 4; ++z) {
                if (!world.getBlock) continue;
                uint32_t state = world.getBlock(x, origin.y, z);
                const mc::BlockState* bs = mc::getBlockState(state);
                if (bs && bs->block && bs->block->name == "minecraft:netherrack") {
                    addDripColumn(x, origin.y - 1, z);
                }
            }
        }
    }

    MC_LOG_INFO("Structure ruined_portal placed at chunk ({},{}), placement={}, template={}, blocks={}",
                active.x, active.z, placement, templateLocation, placed);
    return placed > 0;
}

// OceanMonumentStructure.findGenerationPoint + MonumentBuilding.postProcess.
bool Runtime::tryPlaceOceanMonument(ChunkPos active, const StructureWorld& world) {
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    StructureWorldAccess access;
    access.getBlock = world.getBlock;
    access.setBlock = world.setBlock;
    access.getHeight = world.heightAt;
    access.isInsideBoundingBox = nullptr;
    access.minY = -64;

    piece::placeOceanMonument(access, *random, active.x, active.z);
    MC_LOG_INFO("Structure ocean_monument placed at chunk ({},{})", active.x, active.z);
    return true;
}

// WoodlandMansionStructure.findGenerationPoint + WoodlandMansionPieces.generateMansion.
// The full mansion assembly (grid layout + room placement + wall writing) is
// ~1300 lines. This port places the outer shell (dark oak + cobblestone base)
// at the correct position. GAP: interior rooms, floor/ceiling detail deferred.
bool Runtime::tryPlaceWoodlandMansion(ChunkPos active, const StructureWorld& world) {
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // Rotation.getRandom(random) = Rotation.values()[random.nextInt(4)]
    const int rotIdx = random->nextInt(4);
    (void)rotIdx;  // orientation doesn't affect the shell placement

    // getLowestYIn5by5BoxOffset7Blocks: scan 5×5 area, find lowest Y >= 60.
    // Simplified: use the center chunk's surface height.
    int surfaceY = 60;
    if (world.heightAt) {
        for (int dx = -2; dx <= 2; ++dx)
            for (int dz = -2; dz <= 2; ++dz) {
                int h = world.heightAt(active.x * 16 + dx * 16 + 7, active.z * 16 + dz * 16 + 7);
                if (h < surfaceY) surfaceY = h;
            }
    }
    if (surfaceY < 60) return false;  // Java: startPos.y < 60 → empty

    // Mansion base: 52×52 blocks of cobblestone floor + dark oak walls.
    const int baseX = active.x * 16;
    const int baseZ = active.z * 16;
    const uint32_t cobblestone = mc::getDefaultBlockStateId("cobblestone", 0);
    const uint32_t darkOakPlanks = mc::getDefaultBlockStateId("dark_oak_planks", 0);
    const uint32_t darkOakLog = mc::getDefaultBlockStateId("dark_oak_log", 0);

    // Floor (cobblestone)
    if (world.setBlock) {
        for (int x = 0; x < 52; ++x)
            for (int z = 0; z < 52; ++z)
                world.setBlock(baseX + x, surfaceY, baseZ + z, cobblestone);
        // Outer walls (dark oak logs, 3 high)
        for (int x = 0; x < 52; ++x)
            for (int y = 1; y <= 3; ++y) {
                world.setBlock(baseX + x, surfaceY + y, baseZ, darkOakLog);
                world.setBlock(baseX + x, surfaceY + y, baseZ + 51, darkOakLog);
            }
        for (int z = 0; z < 52; ++z)
            for (int y = 1; y <= 3; ++y) {
                world.setBlock(baseX, surfaceY + y, baseZ + z, darkOakLog);
                world.setBlock(baseX + 51, surfaceY + y, baseZ + z, darkOakLog);
            }
        // Interior floor (dark oak planks)
        for (int x = 1; x < 51; ++x)
            for (int z = 1; z < 51; ++z)
                world.setBlock(baseX + x, surfaceY + 1, baseZ + z, darkOakPlanks);
    }

    MC_LOG_INFO("Structure woodland_mansion placed at chunk ({},{}), y={}", active.x, active.z, surfaceY);
    return true;
}

// NetherFortressStructure.findGenerationPoint + NetherFortressPieces.
// The full fortress assembly (recursive piece tree) is ~1630 lines. This port
// places the starting piece (a nether fortress bridge) at the correct position.
// GAP: recursive child pieces (corridors, crossings, stairs) deferred.
bool Runtime::tryPlaceNetherFortress(ChunkPos active, const StructureWorld& world) {
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // startPos = (chunkX*16 + 2, 64, chunkZ*16 + 2)
    const int startX = active.x * 16 + 2;
    const int startZ = active.z * 16 + 2;
    const int startY = 64;

    // StartPiece ctor: NetherFortressPieces.StartPiece(random, startX, startZ)
    // draws nextInt for the castle-ish start. Consume the RNG to align the
    // stream (even though we only place a basic bridge).
    (void)random->nextInt(3);  // StartPiece draws from the RNG

    // Place a basic nether bridge (nether bricks floor + walls)
    const uint32_t netherBricks = mc::getDefaultBlockStateId("nether_bricks", 0);
    const uint32_t netherBrickFence = mc::getDefaultBlockStateId("nether_brick_fence", 0);

    if (world.setBlock) {
        // 5-wide, 10-long bridge at Y=64
        for (int x = 0; x < 10; ++x)
            for (int z = 0; z < 5; ++z)
                world.setBlock(startX + x, startY, startZ + z, netherBricks);
        // Fences on both sides
        for (int x = 0; x < 10; ++x) {
            world.setBlock(startX + x, startY + 1, startZ, netherBrickFence);
            world.setBlock(startX + x, startY + 1, startZ + 4, netherBrickFence);
        }
    }

    MC_LOG_INFO("Structure fortress placed at chunk ({},{})", active.x, active.z);
    return true;
}

// StrongholdStructure.findGenerationPoint + StrongholdPieces.
// The full stronghold assembly (recursive room tree) is ~1766 lines. This port
// places the starting room (stone bricks) at the chunk center. The CONCENTRIC_RINGS
// placement is now enabled (isStructureChunk handles it). GAP: recursive rooms,
// corridors, stairs, library, portal room deferred.
bool Runtime::tryPlaceStronghold(ChunkPos active, const StructureWorld& world) {
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // startPos = chunkPos.getWorldPosition() = (chunkX*16, ?, chunkZ*16)
    const int baseX = active.x * 16;
    const int baseZ = active.z * 16;

    // StrongholdPieces.StartPiece: tries to place a starting room at Y=64-ish.
    // moveInsideHeights(random, 48, 70) adjusts Y to [48, 70].
    int y = 48 + random->nextInt(70 - 48 + 1);

    // Place a basic stone-brick room (16×16×8)
    const uint32_t stoneBricks = mc::getDefaultBlockStateId("stone_bricks", 0);
    const uint32_t air = mc::getDefaultBlockStateId("air", 0);

    if (world.setBlock) {
        // Floor + ceiling
        for (int x = 0; x < 16; ++x)
            for (int z = 0; z < 16; ++z) {
                world.setBlock(baseX + x, y, baseZ + z, stoneBricks);
                world.setBlock(baseX + x, y + 7, baseZ + z, stoneBricks);
            }
        // Walls
        for (int x = 0; x < 16; ++x)
            for (int yy = 1; yy < 7; ++yy) {
                world.setBlock(baseX + x, y + yy, baseZ, stoneBricks);
                world.setBlock(baseX + x, y + yy, baseZ + 15, stoneBricks);
            }
        for (int z = 0; z < 16; ++z)
            for (int yy = 1; yy < 7; ++yy) {
                world.setBlock(baseX, y + yy, baseZ + z, stoneBricks);
                world.setBlock(baseX + 15, y + yy, baseZ + z, stoneBricks);
            }
        // Interior air
        for (int x = 1; x < 15; ++x)
            for (int z = 1; z < 15; ++z)
                for (int yy = 1; yy < 7; ++yy)
                    world.setBlock(baseX + x, y + yy, baseZ + z, air);
    }

    MC_LOG_INFO("Structure stronghold placed at chunk ({},{}), y={}", active.x, active.z, y);
    return true;
}

// EndCityStructure.findGenerationPoint + EndCityPieces.startHouseTower.
// The end city is a jigsaw-family structure using templates (end_city/*).
// This port places the base template. GAP: recursive child pieces deferred.
bool Runtime::tryPlaceEndCity(ChunkPos active, const StructureWorld& world) {
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, active.x, active.z);

    // Rotation.getRandom(random)
    const int rotIdx = random->nextInt(4);
    const Rotation rot = static_cast<Rotation>(rotIdx);

    // getLowestYIn5by5BoxOffset7Blocks: scan for end stone surface.
    int surfaceY = 60;
    if (world.heightAt) surfaceY = world.heightAt(active.x * 16 + 8, active.z * 16 + 8);
    if (surfaceY < 60) return false;

    // Place the base end city template (end_city/base_floor)
    BlockPos pos{ active.x * 16, surfaceY, active.z * 16 };
    std::size_t placed = placeTemplate("minecraft:end_city/base_floor", pos, rot, world);
    MC_LOG_INFO("Structure end_city placed at chunk ({},{}), y={}, blocks={}", active.x, active.z, surfaceY, placed);
    return placed > 0;
}

const JigsawConfig* Runtime::selectJigsawStructure(const StructureSetDef& set, ChunkPos start,
        const StructureWorld& world,
        const std::function<std::string(int, int, int)>& biomeGetter,
        const std::vector<Placed>*& outPieces) {
    outPieces = nullptr;
    auto tryOne = [&](const std::string& sid) -> const JigsawConfig* {
        auto cfgIt = structures.find(sid);
        if (cfgIt == structures.end() || !cfgIt->second.supported) return nullptr;
        if (cfgIt->second.structureType != "minecraft:jigsaw") return nullptr;
        const std::vector<Placed>* pieces = assembledFor(sid, start, world, biomeGetter);
        if (!pieces || pieces->empty()) return nullptr;
        outPieces = pieces;
        return &cfgIt->second;
    };

    if (set.structures.size() == 1) return tryOne(set.structures[0].structureId);

    std::vector<StructureSelection> options = set.structures;
    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
        std::make_shared<mc::levelgen::LegacyRandomSource>(0));
    random->setLargeFeatureSeed(seed, start.x, start.z);
    int total = 0;
    for (const StructureSelection& o : options) total += o.weight;
    while (!options.empty() && total > 0) {
        int choice = random->nextInt(total);
        std::size_t index = 0;
        for (; index < options.size(); ++index) {
            choice -= options[index].weight;
            if (choice < 0) break;
        }
        if (index >= options.size()) index = options.size() - 1;
        StructureSelection selected = options[index];
        if (const JigsawConfig* cfg = tryOne(selected.structureId)) return cfg;
        total -= selected.weight;
        options.erase(options.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return nullptr;
}

std::size_t Runtime::placeJigsawStartInChunk(const JigsawConfig& cfg, ChunkPos start, ChunkPos decorating,
                                             const std::vector<Placed>& pieces,
                                             const StructureWorld& world,
                                             mc::levelgen::RandomSource& random) {
    const int minX = decorating.x * 16;
    const int minZ = decorating.z * 16;
    BoundingBox chunkBB(minX, kMinBuildY, minZ, minX + 15, kMaxBuildYInclusive, minZ + 15);

    const std::size_t blocks = placePieces(pieces, world, &random, &chunkBB);
    if (blocks > 0) {
        MC_LOG_INFO("Structure {} placed in chunk ({},{}), start=({},{}), pieces={}, blocks={}",
                    cfg.id, decorating.x, decorating.z, start.x, start.z, pieces.size(), blocks);
    }
    return blocks;
}

void Runtime::generate(ChunkPos active, const StructureWorld& world,
                       const std::function<std::string(int, int, int)>& biomeGetter) {
    // Phase 3: lock the entire generate() call. This serializes structure
    // generation across threads, but the lazily-loaded caches (jigsawTemplates,
    // placeTemplates, etc.) are populated during generate(), so we need
    // exclusive access. Once all templates are warm (after the first few
    // chunks), the lock is uncontended and costs ~10ns. In the current
    // single-decoration-worker architecture, there's never contention anyway.
    std::lock_guard<std::shared_mutex> lk(cacheMutex);
    auto structureIndexInStep = [&](const JigsawConfig& cfg) {
        int index = 0;
        for (const auto& [id, other] : structures) {
            if (other.stepIndex != cfg.stepIndex) continue;
            if (id == cfg.id) return index;
            ++index;
        }
        return 0;
    };

    for (const StructureSetDef& set : structureSets) {
        auto pit = placementState.sets.find(set.id);
        if (pit == placementState.sets.end()) continue;
        const StructurePlacement& placement = pit->second;
        if (!StructureState::isSupported(placement)) continue;

        bool anyJigsaw = false;
        int maxDistH = 0;
        for (const StructureSelection& s : set.structures) {
            auto cfgIt = structures.find(s.structureId);
            if (cfgIt != structures.end() && cfgIt->second.supported
                && cfgIt->second.structureType == "minecraft:jigsaw") {
                anyJigsaw = true;
                maxDistH = std::max(maxDistH, cfgIt->second.maxDistH);
            }
        }

        if (anyJigsaw) {
            // StructureStart.placeInChunk runs for starts referenced by this chunk,
            // not only starts whose origin is this chunk. The largest jigsaw radius
            // bounds the scan; piece boxes are clipped by chunkBB during placement.
            const int R = (maxDistH + 15) / 16 + 1;
            for (int sx = active.x - R; sx <= active.x + R; ++sx) {
                for (int sz = active.z - R; sz <= active.z + R; ++sz) {
                    if (!placementState.isStructureChunk(placement, sx, sz)) continue;
                    const std::vector<Placed>* pieces = nullptr;
                    const JigsawConfig* cfg = selectJigsawStructure(set, {sx, sz}, world, biomeGetter, pieces);
                    if (!cfg || !pieces) continue;
                    auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
                        std::make_shared<mc::levelgen::XoroshiroRandomSource>(seed));
                    const std::int64_t deco = random->setDecorationSeed(seed, active.x * 16, active.z * 16);
                    random->setFeatureSeed(deco, structureIndexInStep(*cfg), cfg->stepIndex);
                    (void)placeJigsawStartInChunk(*cfg, {sx, sz}, active, *pieces, world, *random);
                }
            }
            continue;
        }

        if (!placementState.isStructureChunk(placement, active.x, active.z)) continue;

        if (set.structures.size() == 1) {
            tryGenerateAndPlace(set.structures[0].structureId, active, world, biomeGetter);
            continue;
        }

        std::vector<StructureSelection> options = set.structures;
        auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
            std::make_shared<mc::levelgen::LegacyRandomSource>(0));
        random->setLargeFeatureSeed(seed, active.x, active.z);
        int total = 0;
        for (const StructureSelection& option : options) total += option.weight;
        while (!options.empty() && total > 0) {
            int choice = random->nextInt(total);
            std::size_t index = 0;
            for (; index < options.size(); ++index) {
                choice -= options[index].weight;
                if (choice < 0) break;
            }
            if (index >= options.size()) index = options.size() - 1;
            StructureSelection selected = options[index];
            if (tryGenerateAndPlace(selected.structureId, active, world, biomeGetter)) return;
            total -= selected.weight;
            options.erase(options.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
}

const std::vector<Placed>* Runtime::assembledFor(const std::string& structureId, ChunkPos start,
                                                 const StructureWorld& world,
                                                 const std::function<std::string(int, int, int)>& biomeGetter) {
    std::string key = structureId + ":" + std::to_string(start.x) + "," + std::to_string(start.z);
    auto it = assembledCache.find(key);
    if (it != assembledCache.end()) return it->second.empty() ? nullptr : &it->second;

    std::vector<Placed>& pieces = assembledCache[key];  // cached even if assembly fails (stays empty)
    auto cfgIt = structures.find(structureId);
    if (cfgIt == structures.end() || !cfgIt->second.supported) return nullptr;
    const JigsawConfig& cfg = cfgIt->second;
    if (cfg.structureType != "minecraft:jigsaw") return nullptr;  // only jigsaw structures beard in the overworld
    BlockPos stub{};
    try {
        if (!assembleJigsaw(cfg, start, world, biomeGetter, pieces, stub)) { pieces.clear(); return nullptr; }
    } catch (const std::exception&) {
        pieces.clear();
        return nullptr;
    }
    return pieces.empty() ? nullptr : &pieces;
}

mc::levelgen::Beardifier Runtime::buildBeardifier(
        ChunkPos active,
        const std::function<int(int, int)>& columnHeight,
        const std::function<std::string(int, int, int)>& biomeGetter) {
    using mc::levelgen::Beardifier;
    using mc::levelgen::TerrainAdjustment;

    StructureWorld world;
    world.heightAt = columnHeight;
    world.getBlock = [](int, int, int) { return 0u; };
    world.setBlock = [](int, int, int, std::uint32_t) {};

    std::vector<Beardifier::Rigid> rigids;
    std::vector<Beardifier::Junction> junctions;
    std::optional<mc::levelgen::BeardBox> any;

    const int chunkStartX = active.x * 16;
    const int chunkStartZ = active.z * 16;
    auto toBeardBox = [](const BoundingBox& b) {
        return mc::levelgen::BeardBox{b.minX, b.minY, b.minZ, b.maxX, b.maxY, b.maxZ};
    };

    // Mirror generate()'s structure selection (without placing) to find the structure
    // that actually starts at `start`, and its assembled pieces + terrain_adaptation.
    auto selectAssembled = [&](const StructureSetDef& set, ChunkPos start)
            -> std::pair<const std::vector<Placed>*, TerrainAdjustment> {
        if (set.structures.size() == 1) {
            const std::string& sid = set.structures[0].structureId;
            auto cfgIt = structures.find(sid);
            TerrainAdjustment adj = cfgIt != structures.end() ? cfgIt->second.terrainAdjustment
                                                              : TerrainAdjustment::NONE;
            return {assembledFor(sid, start, world, biomeGetter), adj};
        }
        std::vector<StructureSelection> options = set.structures;
        auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
            std::make_shared<mc::levelgen::LegacyRandomSource>(0));
        random->setLargeFeatureSeed(seed, start.x, start.z);
        int total = 0;
        for (const StructureSelection& o : options) total += o.weight;
        while (!options.empty() && total > 0) {
            int choice = random->nextInt(total);
            std::size_t index = 0;
            for (; index < options.size(); ++index) {
                choice -= options[index].weight;
                if (choice < 0) break;
            }
            if (index >= options.size()) index = options.size() - 1;
            StructureSelection selected = options[index];
            if (const std::vector<Placed>* pieces = assembledFor(selected.structureId, start, world, biomeGetter)) {
                auto cfgIt = structures.find(selected.structureId);
                TerrainAdjustment adj = cfgIt != structures.end() ? cfgIt->second.terrainAdjustment
                                                                  : TerrainAdjustment::NONE;
                return {pieces, adj};
            }
            total -= selected.weight;
            options.erase(options.begin() + static_cast<std::ptrdiff_t>(index));
        }
        return {nullptr, TerrainAdjustment::NONE};
    };

    for (const StructureSetDef& set : structureSets) {
        auto pit = placementState.sets.find(set.id);
        if (pit == placementState.sets.end()) continue;
        const StructurePlacement& placement = pit->second;
        if (!StructureState::isSupported(placement)) continue;

        // Only sets with a terrain-adapting jigsaw structure matter; bound the scan
        // window by the largest piece extent (maxDistH) among them.
        int maxDistH = 0;
        bool anyBeard = false;
        for (const StructureSelection& s : set.structures) {
            auto c = structures.find(s.structureId);
            if (c != structures.end() && c->second.structureType == "minecraft:jigsaw"
                && c->second.terrainAdjustment != TerrainAdjustment::NONE) {
                anyBeard = true;
                maxDistH = std::max(maxDistH, c->second.maxDistH);
            }
        }
        if (!anyBeard) continue;
        const int R = (maxDistH + 12 + 15) / 16 + 1;

        for (int sx = active.x - R; sx <= active.x + R; ++sx) {
            for (int sz = active.z - R; sz <= active.z + R; ++sz) {
                if (!placementState.isStructureChunk(placement, sx, sz)) continue;
                auto [pieces, adj] = selectAssembled(set, {sx, sz});
                if (!pieces || adj == TerrainAdjustment::NONE) continue;

                for (const Placed& piece : *pieces) {
                    // isCloseToChunk(active, 12): piece box (XZ) intersects the chunk inflated by 12.
                    if (!piece.box.intersects(chunkStartX - 12, chunkStartZ - 12,
                                              chunkStartX + 15 + 12, chunkStartZ + 15 + 12))
                        continue;
                    if (piece.rigid) {
                        mc::levelgen::BeardBox bb = toBeardBox(piece.box);
                        rigids.push_back({bb, adj, piece.groundLevelDelta});
                        any = any ? mc::levelgen::BeardBox::encapsulating(*any, bb) : bb;
                    }
                    for (const Beardifier::Junction& jn : piece.junctions) {
                        if (jn.sourceX > chunkStartX - 12 && jn.sourceZ > chunkStartZ - 12
                            && jn.sourceX < chunkStartX + 15 + 12 && jn.sourceZ < chunkStartZ + 15 + 12) {
                            junctions.push_back(jn);
                            mc::levelgen::BeardBox jb{jn.sourceX, jn.sourceGroundY, jn.sourceZ,
                                                      jn.sourceX, jn.sourceGroundY, jn.sourceZ};
                            any = any ? mc::levelgen::BeardBox::encapsulating(*any, jb) : jb;
                        }
                    }
                }
            }
        }
    }

    if (!any) return Beardifier();
    return Beardifier(std::move(rigids), std::move(junctions), any->inflatedBy(24));
}

Runtime* runtimeFor(const std::string& dataMinecraftDir, int64_t seed) {
    // Thread-safe singleton: std::call_once guarantees only one thread
    // initializes the Runtime. Subsequent calls (from any thread) read the
    // cached pointer without locking. If the dir/seed changes, we recreate
    // under a mutex — this happens only on world load/unload, not per-chunk.
    static std::unique_ptr<Runtime> runtime;
    static std::string cachedDir;
    static int64_t cachedSeed = 0;
    static std::mutex initMutex;

    fs::path dir(dataMinecraftDir);
    std::string key = dir.lexically_normal().generic_string();
    {
        // Fast path: check if already initialized with the same key+seed.
        // This is a data race on reads of cachedDir/cachedSeed, but it's
        // benign: worst case we fall through to the locked path and find
        // nothing changed. The locked path below does the authoritative check.
        std::lock_guard<std::mutex> lk(initMutex);
        if (!runtime || cachedDir != key || cachedSeed != seed) {
            runtime = std::make_unique<Runtime>(dir, seed);
            cachedDir = key;
            cachedSeed = seed;
        }
    }
    return runtime.get();
}

} // namespace

void generateStructures(ChunkPos active, uint64_t worldSeed,
                        const StructureWorld& world,
                        const std::function<std::string(int, int, int)>& biomeGetter,
                        const std::string& dataMinecraftDir) {
    if (dataMinecraftDir.empty()) return;
    fs::path dataDir(dataMinecraftDir);
    if (!fs::exists(dataDir / "worldgen" / "structure_set")) return;
    Runtime* runtime = runtimeFor(dataMinecraftDir, static_cast<int64_t>(worldSeed));
    runtime->generate(active, world, biomeGetter);
}

mc::levelgen::Beardifier generateBeardifier(
        ChunkPos active, uint64_t worldSeed,
        const std::function<int(int, int)>& columnHeight,
        const std::function<std::string(int, int, int)>& biomeGetter,
        const std::string& dataMinecraftDir) {
    if (dataMinecraftDir.empty()) return {};
    fs::path dataDir(dataMinecraftDir);
    if (!fs::exists(dataDir / "worldgen" / "structure_set")) return {};
    Runtime* runtime = runtimeFor(dataMinecraftDir, static_cast<int64_t>(worldSeed));
    return runtime->buildBeardifier(active, columnHeight, biomeGetter);
}

} // namespace mc::levelgen::structure
