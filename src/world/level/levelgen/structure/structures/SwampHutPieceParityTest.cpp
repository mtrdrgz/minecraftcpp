// Bit-exact parity gate for the block-placement body of the REAL
//   net.minecraft...structures.SwampHutPiece.postProcess
// ported in
//   world/level/levelgen/structure/structures/SwampHutPiece.h
// (StructurePieceBase helper methods + the piece-specific postProcess logic).
//
// Ground truth (tools/SwampHutPieceParity.java) drives the REAL
// SwampHutPiece.postProcess against a CAPTURING WorldGenLevel proxy (returns
// Y=64 for getHeightmapPos, no-ops getChunk / markPosForPostprocessing, returns
// null for getLevel so entity spawns short-circuit). The C++ side replays the
// same setup against our SwampHutPiece + a StructureWorldAccess that records
// every setBlock. Each (wx, wy, wz, blockName, props) tuple is compared.
//
//   swamp_hut_piece_parity --cases build/swamp_hut_piece.tsv
//                           --states src/assets/block_states.json
//
// TSV rows (decimal ints; props is comma-joined k=v alphabetical):
//   BOX    <seed> <west> <north> <minX> <minY> <minZ> <maxX> <maxY> <maxZ> <dirOrd> <rotOrd> <mirOrd>
//   PLACE  <seed> <west> <north> <wx> <wy> <wz> <blockName> <props>
//   COUNT  <seed> <west> <north> <numPlaced>

#include "world/level/levelgen/structure/structures/SwampHutPiece.h"
#include "world/level/levelgen/structure/ScatteredFeaturePieceBox.h"
#include "world/level/levelgen/structure/StructurePieceBase.h"
#include "world/level/levelgen/RandomSource.h"
#include "world/level/block/Blocks.h"
#include "world/level/block/BlockRotation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace piece = mc::levelgen::structure::piece;
namespace structure = mc::levelgen::structure;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::block_rotation::StateInfo;
using mc::block_rotation::Lookup;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

int i32(const std::string& s) { return static_cast<int>(std::stoll(s)); }
int64_t i64(const std::string& s) { return static_cast<int64_t>(std::stoll(s)); }

// Canonicalize a props string ("facing=north,shape=straight,waterlogged=false")
// to alphabetical-by-key form. Empty input -> empty output.
std::string canonicalProps(const std::string& props) {
    if (props.empty()) return "";
    std::vector<std::pair<std::string, std::string>> kvs;
    std::stringstream ss(props);
    std::string item;
    while (std::getline(ss, item, ',')) {
        auto eq = item.find('=');
        if (eq == std::string::npos) continue;
        kvs.emplace_back(item.substr(0, eq), item.substr(eq + 1));
    }
    std::sort(kvs.begin(), kvs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::string out;
    for (const auto& [k, v] : kvs) {
        if (!out.empty()) out += ',';
        out += k + '=' + v;
    }
    return out;
}

// Per-id (name, props-string) — props-string is the JSON `props` field as
// stored (comma-joined k=v, NOT necessarily alphabetical). Also exposes the
// BlockRotation-compatible StateInfo vector and a (name, props) -> stateId
// reverse index so the transformState hook can do mirror/rotate lookups.
struct StateTable {
    std::vector<std::pair<std::string, std::string>> byId;
    std::vector<StateInfo> states;       // BlockRotation-compatible
    std::unordered_map<std::string, uint32_t> revIndex;
};

// Populate the engine's g_blockStates + g_defaultStateByName + g_blocksByName
// (with lightweight Block* per distinct block name) from block_states.json so
// the SwampHutPiece port's getDefaultBlockStateId() / getBlockStateIdWith()
// resolve to real state IDs (not 0=air fallback) during the test.
StateTable loadAndPopulateStates(const std::string& path) {
    StateTable t;
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << path << "\n"; std::exit(2); }
    nlohmann::json j; f >> j;
    auto arr = j.at("states");
    std::size_t maxId = 0;
    for (auto& s : arr) maxId = std::max<std::size_t>(maxId, s.at("id").get<std::size_t>());
    t.byId.assign(maxId + 1, {"", ""});
    t.states.assign(maxId + 1, StateInfo{"", ""});
    mc::g_blockStates.assign(maxId + 1, mc::BlockState{});
    // Owns the lightweight Block instances we synthesize (one per block name).
    std::unordered_map<std::string, std::unique_ptr<mc::Block>> blockByName;
    for (auto& s : arr) {
        uint32_t id = s.at("id").get<uint32_t>();
        std::string name = s.at("name").get<std::string>();
        std::string props = s.value("props", std::string());
        t.byId[id] = {name, props};
        t.states[id] = StateInfo{name, props};
        // Reverse index keyed by name + "\x01" + canonical (alphabetical) props.
        t.revIndex[name + "\x01" + canonicalProps(props)] = id;
        mc::BlockState& bs = mc::g_blockStates[id];
        bs.stateId = id;
        // Find or create a fake Block for this name.
        auto bit = blockByName.find(name);
        if (bit == blockByName.end()) {
            mc::Block::Properties p{};
            // Populate isAir/isFluid so StructurePieceBase::isReplaceableByStructures
            // (which calls bs->block->isAir() / isFluid()) works correctly. Without
            // this, fillColumnDown would see air as non-replaceable and stop early.
            p.isAir = s.value("is_air", false);
            p.isFluid = s.value("is_fluid", false);
            auto b = std::make_unique<mc::Block>(p);
            b->name = name;
            blockByName[name] = std::move(b);
            bit = blockByName.find(name);
            mc::g_blocksByName[name] = bit->second.get();
        }
        bs.block = bit->second.get();
        if (!props.empty()) {
            bs.props = props;
            size_t start = 0;
            while (start < props.size()) {
                size_t comma = props.find(',', start);
                if (comma == std::string::npos) comma = props.size();
                size_t eq = props.find('=', start);
                if (eq != std::string::npos && eq < comma) {
                    bs.properties.emplace(props.substr(start, eq - start),
                                          props.substr(eq + 1, comma - eq - 1));
                }
                start = comma + 1;
            }
        }
        if (s.value("default", false)) {
            mc::g_defaultStateByName[name] = id;
        }
    }
    // Transfer ownership of the synthesized Block instances to g_blockStorage
    // so they outlive this function.
    for (auto& [_, bptr] : blockByName) {
        mc::g_blockStorage.push_back(std::move(bptr));
    }
    return t;
}

// Load the FAM (declaring-class) table produced by BlockRotateMirrorParity.java.
// Map: blockName (without "minecraft:") -> (rotFam, mirFam).
std::unordered_map<std::string, std::pair<std::string, std::string>>
loadFam(const std::string& path) {
    std::unordered_map<std::string, std::pair<std::string, std::string>> fam;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "cannot open " << path << " — run block_rotate_mirror_parity GT first\n";
        std::exit(2);
    }
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto c = mc::block_rotation::splitc(line, '\t');
        if (c.size() >= 4 && c[0] == "FAM") {
            std::string key = c[1];
            auto colon = key.find(':');
            if (colon != std::string::npos) key = key.substr(colon + 1);
            fam[key] = {c[2], c[3]};
        }
    }
    return fam;
}

// A captured block placement: (wx, wy, wz, stateId).
struct Placed {
    int wx, wy, wz;
    uint32_t stateId;
};

// Build a StructureWorldAccess that records every setBlock in `out`. The
// transformState hook wires Java's BlockState.mirror().rotate() through the
// certified BlockRotation.h, using the FAM (declaring class) per block name
// and a (name, props) -> stateId reverse lookup built from block_states.json.
//
// The `placed` map (shared with setBlock) is also used by getBlock so the
// isReplaceableByStructures check inside fillColumnDown sees prior writes —
// matching the Java capturing level's behavior (Java's proxy returns the
// last-written state for getBlockState, NOT always air).
structure::StructureWorldAccess makeCapturingWorld(
        std::vector<Placed>& out,
        std::map<std::tuple<int, int, int>, uint32_t>& placed,
        const std::vector<StateInfo>& states,
        const std::unordered_map<std::string, uint32_t>& revIndex,
        const std::unordered_map<std::string, std::pair<std::string, std::string>>& fam) {
    structure::StructureWorldAccess w;
    w.getBlock = [&](int x, int y, int z) -> uint32_t {
        auto it = placed.find({x, y, z});
        return it == placed.end() ? 0u : it->second;
    };
    w.setBlock = [&](int x, int y, int z, uint32_t state) {
        placed[{x, y, z}] = state;
        out.push_back({x, y, z, state});
    };
    w.getHeight = [](int, int) -> int { return 64; };
    w.isInsideBoundingBox = nullptr;
    w.minY = -64;

    // The lookup closure captures revIndex by reference.
    Lookup lookup = [&](const std::string& name, const std::string& props) -> long {
        auto it = revIndex.find(name + "\x01" + props);
        return it == revIndex.end() ? -1 : (long)it->second;
    };
    w.transformState = [&](uint32_t stateId, piece::Mirror mir, piece::Rotation rot) -> uint32_t {
        if (stateId >= states.size()) return stateId;
        const StateInfo& si = states[stateId];
        // Look up the declaring class for this block name.
        auto fi = fam.find(si.name);
        std::string rotFam = (fi == fam.end()) ? "BlockBehaviour" : fi->second.first;
        std::string mirFam = (fi == fam.end()) ? "BlockBehaviour" : fi->second.second;
        // piece::Mirror/Rotation share ordinal values with structure::Mirror/Rotation
        // (both are NONE=0, LEFT_RIGHT=1, FRONT_BACK=2 / NONE=0, CW90=1, CW180=2, CCW90=3).
        // Cast through the underlying int.
        auto castMir = [](piece::Mirror m) {
            return static_cast<Mirror>(static_cast<int>(m));
        };
        auto castRot = [](piece::Rotation r) {
            return static_cast<Rotation>(static_cast<int>(r));
        };
        Mirror emir = castMir(mir);
        Rotation erot = castRot(rot);
        // Java applies mirror FIRST, then rotate.
        long cur = (long)stateId;
        if (emir != Mirror::NONE) {
            long m = mc::block_rotation::mirror(si, cur, emir, mirFam, lookup);
            if (m >= 0) cur = m;
        }
        if (erot != Rotation::NONE) {
            // Re-fetch StateInfo for the (possibly mirrored) state.
            if ((uint32_t)cur < states.size()) {
                const StateInfo& curSi = states[(uint32_t)cur];
                long r = mc::block_rotation::rotate(curSi, cur, erot, rotFam, lookup);
                if (r >= 0) cur = r;
            }
        }
        return (uint32_t)cur;
    };
    return w;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    std::string statesPath = "src/assets/block_states.json";
    std::string famPath = "build/block_rotate_mirror.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
        else if (a == "--fam" && i + 1 < argc) famPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: swamp_hut_piece_parity --cases <tsv> [--states <json>] [--fam <tsv>]\n";
        return 2;
    }

    StateTable table = loadAndPopulateStates(statesPath);
    auto fam = loadFam(famPath);

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long boxChecks = 0, placeChecks = 0, countChecks = 0;
    long long boxMism = 0, placeMism = 0, countMism = 0;
    int shown = 0;

    struct ExpPlace { int wx, wy, wz; std::string name; std::string props; };
    std::map<std::tuple<int64_t, int, int>, std::vector<ExpPlace>> expPlaces;
    std::map<std::tuple<int64_t, int, int>, int> expCounts;
    std::map<std::tuple<int64_t, int, int>, std::array<int, 9>> expBoxes;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        if (tag == "BOX" && p.size() >= 13) {
            int64_t seed = i64(p[1]); int west = i32(p[2]); int north = i32(p[3]);
            std::array<int, 9> v;
            for (int i = 0; i < 9; ++i) v[i] = i32(p[4 + i]);
            expBoxes[{seed, west, north}] = v;
        } else if (tag == "PLACE" && p.size() >= 8) {
            // p.size() is 8 when the trailing props field is empty (line ends
            // with `\t`); std::getline doesn't emit a final empty element in
            // that case. Treat both 8 (no props) and 9 (with props) uniformly.
            int64_t seed = i64(p[1]); int west = i32(p[2]); int north = i32(p[3]);
            std::string props = (p.size() >= 9) ? p[8] : "";
            ExpPlace e{ i32(p[4]), i32(p[5]), i32(p[6]), p[7], props };
            expPlaces[{seed, west, north}].push_back(e);
        } else if (tag == "COUNT" && p.size() >= 5) {
            int64_t seed = i64(p[1]); int west = i32(p[2]); int north = i32(p[3]);
            expCounts[{seed, west, north}] = i32(p[4]);
        }
    }

    for (const auto& [key, expBox] : expBoxes) {
        auto [seed, west, north] = key;
        ++boxChecks;

        auto random = mc::levelgen::RandomSource::create(seed);
        piece::SwampHutPiece hut(*random, west, north);

        // The ctor advances the random by one nextInt(4) draw — mirror that on
        // the postProcess-side random state (matches the Java side which does
        // r2.nextInt(4) before calling postProcess).
        auto random2 = mc::levelgen::RandomSource::create(seed);
        random2->nextInt(4);

        std::vector<Placed> placedVec;
        std::map<std::tuple<int, int, int>, uint32_t> placedMap;
        auto world = makeCapturingWorld(placedVec, placedMap, table.states, table.revIndex, fam);
        hut.postProcess(world);

        // Build actual placements: (wx,wy,wz) -> (name, props) using our state
        // table. Last-write-wins (matches Java's LinkedHashMap put semantics).
        std::map<std::tuple<int, int, int>, std::pair<std::string, std::string>> actual;
        for (const Placed& pl : placedVec) {
            if (pl.stateId >= table.byId.size()) continue;
            const auto& [name, props] = table.byId[pl.stateId];
            actual[{pl.wx, pl.wy, pl.wz}] = {name, canonicalProps(props)};
        }

        const auto& expList = expPlaces[key];
        std::map<std::tuple<int, int, int>, std::pair<std::string, std::string>> expected;
        for (const ExpPlace& e : expList) {
            expected[{e.wx, e.wy, e.wz}] = {e.name, e.props};
        }

        std::set<std::tuple<int, int, int>> keys;
        for (const auto& [k, _] : actual)  keys.insert(k);
        for (const auto& [k, _] : expected) keys.insert(k);

        for (const auto& k : keys) {
            ++placeChecks;
            auto aIt = actual.find(k);
            auto eIt = expected.find(k);
            std::string aName = (aIt != actual.end()) ? aIt->second.first  : "<missing>";
            std::string aProps= (aIt != actual.end()) ? aIt->second.second : "";
            std::string eName = (eIt != expected.end()) ? eIt->second.first  : "<missing>";
            std::string eProps= (eIt != expected.end()) ? eIt->second.second : "";
            if (aName != eName || aProps != eProps) {
                ++placeMism;
                if (shown++ < 40) {
                    auto [x, y, z] = k;
                    std::cerr << "MISMATCH seed=" << seed << " west=" << west << " north=" << north
                              << " pos=(" << x << "," << y << "," << z << ")"
                              << " exp=" << eName << "[" << eProps << "]"
                              << " got=" << aName << "[" << aProps << "]\n";
                }
            }
        }

        ++countChecks;
        int expCount = expCounts.count(key) ? expCounts.at(key) : -1;
        if ((int)actual.size() != expCount) {
            ++countMism;
            if (shown++ < 40) {
                std::cerr << "COUNT_MISMATCH seed=" << seed << " west=" << west << " north=" << north
                          << " exp=" << expCount << " got=" << actual.size() << "\n";
            }
        }
    }

    std::cout << "SwampHutPiece boxChecks=" << boxChecks << " placeChecks=" << placeChecks
              << " countChecks=" << countChecks
              << " boxMism=" << boxMism << " placeMism=" << placeMism << " countMism=" << countMism << "\n";
    return (placeMism + countMism + boxMism) == 0 ? 0 : 1;
}
