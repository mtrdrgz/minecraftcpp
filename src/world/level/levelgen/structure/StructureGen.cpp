#include "StructureGen.h"

#include "placement/StructurePlacement.h"
#include "StructurePieceBase.h"
#include "structures/SwampHutPiece.h"
#include "structures/DesertPyramidPiece.h"
#include "structures/JungleTemplePiece.h"
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
#include "world/phys/AABB.h"
#include "world/phys/shapes/Shapes.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
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

struct Runtime {
    fs::path dataDir;
    int64_t seed = 0;
    StructureState placementState;
    StateResolver resolver;
    std::vector<StructureSetDef> structureSets;
    std::map<std::string, JigsawConfig> structures;
    std::map<std::string, pools::StructureTemplatePool> poolMap;
    std::map<std::string, stl::LoadedTemplate> jigsawTemplates;
    std::map<std::string, PlaceTemplate> placeTemplates;
    std::unordered_map<std::string, std::string> oracleTemplateB64;
    std::unordered_set<std::string> loadedPoolRoots;
    std::unordered_set<std::string> missingTemplates;
    std::unordered_set<std::string> biomeTagStack;
    bool oracleTemplateIndexLoaded = false;

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
    std::size_t placePieces(const std::vector<Placed>& pieces, const StructureWorld& world);
    std::size_t placeElement(const pools::StructurePoolElement& e, const BlockPos& pos,
                             Rotation rot, const StructureWorld& world);
    std::size_t placeTemplate(const std::string& location, const BlockPos& pos,
                              Rotation rot, const StructureWorld& world);
    bool tryGenerateAndPlace(const std::string& structureId, ChunkPos active, const StructureWorld& world,
                             const std::function<std::string(int, int, int)>& biomeGetter);
    bool tryPlaceSwampHut(ChunkPos active, const StructureWorld& world);
    bool tryPlaceDesertPyramid(ChunkPos active, const StructureWorld& world);
    bool tryPlaceJungleTemple(ChunkPos active, const StructureWorld& world);
    bool tryPlaceShipwreck(ChunkPos active, const StructureWorld& world, bool isBeached);
    bool tryPlaceIgloo(ChunkPos active, const StructureWorld& world);
    bool tryPlaceNetherFossil(ChunkPos active, const StructureWorld& world);
    void generate(ChunkPos active, const StructureWorld& world,
                  const std::function<std::string(int, int, int)>& biomeGetter);
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
    
    // RULE #0 HONESTY: a structure type is listed here ONLY if it has a real,
    // dispatched piece-placement path in tryGenerateAndPlace(). A type that is
    // recognised but NOT actually placed must NOT be marked supported — otherwise
    // it silently no-ops (failed jigsaw assembly with an empty start_pool) while
    // pretending to be ported. Types deliberately NOT here yet (helpers only, no
    // in-game placement): ocean_ruin, ruined_portal, buried_treasure,
    // ocean_monument, woodland_mansion, mineshaft, stronghold, fortress, end_city.
    // See docs/STRUCTURES_STATUS.md for the per-structure port ledger.
    static const std::set<std::string> supportedTypes = {
        "minecraft:jigsaw",
        "minecraft:swamp_hut",
        "minecraft:desert_pyramid",
        "minecraft:igloo",
        "minecraft:jungle_temple",
        "minecraft:shipwreck",
        "minecraft:nether_fossil",
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

std::size_t Runtime::placeTemplate(const std::string& location, const BlockPos& pos,
                                   Rotation rot, const StructureWorld& world) {
    if (!ensureTemplate(location)) return 0;
    const PlaceTemplate& tpl = placeTemplates.at(location);
    std::size_t placed = 0;
    for (const PlaceBlock& block : tpl.blocks) {
        BlockPos rp = structureTransform(block.pos, Mirror::NONE, rot, kBlockPosZero);
        std::uint32_t state = resolver.rotateState(block.state, rot);
        if (world.setBlock) {
            world.setBlock(pos.x + rp.x, pos.y + rp.y, pos.z + rp.z, state);
            ++placed;
        }
    }
    return placed;
}

std::size_t Runtime::placeElement(const pools::StructurePoolElement& e, const BlockPos& pos,
                                  Rotation rot, const StructureWorld& world) {
    switch (e.type) {
        case pools::ElementType::SINGLE:
        case pools::ElementType::LEGACY:
            return placeTemplate(e.location, pos, rot, world);
        case pools::ElementType::LIST: {
            std::size_t placed = 0;
            for (const auto& child : e.elements) placed += placeElement(child, pos, rot, world);
            return placed;
        }
        case pools::ElementType::FEATURE:
        case pools::ElementType::EMPTY:
        default:
            return 0;
    }
}

std::size_t Runtime::placePieces(const std::vector<Placed>& pieces, const StructureWorld& world) {
    std::size_t blocks = 0;
    for (const Placed& piece : pieces) {
        if (!piece.element) continue;
        blocks += placeElement(*piece.element, piece.position, piece.rotation, world);
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
    // TODO: add more non-jigsaw structure types

    // Jigsaw structure assembly (existing path)
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

void Runtime::generate(ChunkPos active, const StructureWorld& world,
                       const std::function<std::string(int, int, int)>& biomeGetter) {
    for (const StructureSetDef& set : structureSets) {
        auto pit = placementState.sets.find(set.id);
        if (pit == placementState.sets.end()) continue;
        const StructurePlacement& placement = pit->second;
        if (!StructureState::isSupported(placement)) continue;
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

Runtime* runtimeFor(const std::string& dataMinecraftDir, int64_t seed) {
    static std::unique_ptr<Runtime> runtime;
    static std::string cachedDir;
    static int64_t cachedSeed = 0;

    fs::path dir(dataMinecraftDir);
    std::string key = dir.lexically_normal().generic_string();
    if (!runtime || cachedDir != key || cachedSeed != seed) {
        runtime = std::make_unique<Runtime>(dir, seed);
        cachedDir = key;
        cachedSeed = seed;
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

} // namespace mc::levelgen::structure
