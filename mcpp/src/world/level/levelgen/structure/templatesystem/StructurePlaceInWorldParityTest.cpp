// Parity gate for net.minecraft...StructureTemplate.placeInWorld — placing a template's blocks
// into the world (the core of structure generation, #18/#19). Ground truth:
// tools/StructurePlaceInWorldParity.java drives the REAL placeInWorld into a capturing level.
//
// This reproduces the placed (worldPos -> stateId) map for each (template, rotation, mirror,
// position) case using ONLY certified primitives:
//   worldPos  = structureTransform(localPos, mirror, rotation, pivot=ZERO) + position
//               (= StructureTemplate.calculateRelativePosition + offset, structure_transform_parity)
//   finalState = state.mirror(mirror).rotate(rotation)   (BlockRotation.h, block_rotate_mirror_parity)
// The template nbt is parsed directly (palette Name+Properties -> sorted props -> state id; blocks
// -> localPos + palette index). No processors / no waterlogging in the GT, so the map is the pure
// transform of every template block (positions are distinct under the bijective transform, so
// last-write-wins never triggers and iteration order is irrelevant).
//
//   structure_placeinworld_parity [--cases mcpp/build/structure_placeinworld.tsv]
//       [--states mcpp/src/assets/block_states.json] [--fam mcpp/build/block_rotate_mirror.tsv]

#include "../../../block/BlockRotation.h"
#include "StructureTransforms.h"
#include "../../../../../nbt/NbtIo.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::BlockPos;
using mc::levelgen::structure::structureTransform;
using mc::levelgen::structure::kBlockPosZero;
using mc::block_rotation::StateInfo;

namespace {
std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    if (!o.empty() && !o.back().empty() && o.back().back() == '\r') o.back().pop_back();
    return o;
}
std::vector<std::uint8_t> b64decode(const std::string& in) {
    static const std::string A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int tbl[256]; for (int i = 0; i < 256; ++i) tbl[i] = -1;
    for (int i = 0; i < 64; ++i) tbl[(unsigned char)A[i]] = i;
    std::vector<std::uint8_t> out; int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || tbl[c] == -1) continue;
        val = (val << 6) + tbl[c]; bits += 6;
        if (bits >= 0) { out.push_back((std::uint8_t)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}
std::string normId(const std::string& s) { return s.find(':') == std::string::npos ? "minecraft:" + s : s; }

// One template block: local pos + palette index.
struct TBlock { BlockPos pos; int stateIdx; };
struct PaletteEntry { std::string name; std::string sortedProps; };  // sortedProps: "k=v,k=v" (key-sorted)
struct Tmpl { std::vector<PaletteEntry> palette; std::vector<TBlock> blocks; };

// Build the key-sorted props string for a Properties compound.
std::string sortedPropsOf(const mc::nbt::NbtCompound* props) {
    if (!props) return "";
    std::vector<std::string> kv;
    for (const auto& [k, tag] : props->entries) kv.push_back(k + "=" + props->getString(k, ""));
    std::sort(kv.begin(), kv.end());
    std::string out;
    for (std::size_t i = 0; i < kv.size(); ++i) { if (i) out += ','; out += kv[i]; }
    return out;
}
Tmpl parseTemplate(const mc::nbt::NbtCompound& root) {
    Tmpl t;
    if (const mc::nbt::NbtList* pal = root.getList("palette")) {
        for (const auto& e : pal->elements) {
            const auto* cp = std::get_if<std::shared_ptr<mc::nbt::NbtCompound>>(&e.value);
            if (!cp) { t.palette.push_back({"minecraft:air", ""}); continue; }
            const mc::nbt::NbtCompound& c = **cp;
            t.palette.push_back({ normId(c.getString("Name", "minecraft:air")), sortedPropsOf(c.getCompound("Properties")) });
        }
    }
    if (const mc::nbt::NbtList* bl = root.getList("blocks")) {
        for (const auto& e : bl->elements) {
            const auto* bp = std::get_if<std::shared_ptr<mc::nbt::NbtCompound>>(&e.value);
            if (!bp) continue;
            const mc::nbt::NbtCompound& b = **bp;
            BlockPos pos{};
            if (const mc::nbt::NbtList* pl = b.getList("pos"); pl && pl->elements.size() >= 3) {
                auto ai = [](const mc::nbt::NbtTag& tg){ const auto* v = tg.as<std::int32_t>(); return v ? *v : 0; };
                pos = BlockPos{ ai(pl->elements[0]), ai(pl->elements[1]), ai(pl->elements[2]) };
            }
            t.blocks.push_back({ pos, b.getInt("state", 0) });
        }
    }
    return t;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/structure_placeinworld.tsv";
    std::string statesPath = "mcpp/src/assets/block_states.json";
    std::string famPath = "mcpp/build/block_rotate_mirror.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
        else if (a == "--fam" && i + 1 < argc) famPath = argv[++i];
    }

    // block_states.json -> per-id (name, canonical props); canonical + key-sorted reverse indices.
    std::vector<StateInfo> states;
    std::unordered_map<std::string, long> canonRev;   // name|canonicalProps -> id (for BlockRotation lookup)
    std::unordered_map<std::string, long> sortedRev;  // name|sortedProps     -> id (for template lookup)
    {
        std::ifstream f(statesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
        nlohmann::json j; f >> j;
        auto arr = j.at("states");
        states.resize(arr.size());
        for (auto& s : arr) {
            long id = s.at("id").get<long>();
            std::string name = s.at("name").get<std::string>();
            std::string props = s.value("props", std::string());
            states[id] = StateInfo{ name, props };
            canonRev[name + "\x01" + props] = id;
            // key-sorted variant of the canonical props.
            std::vector<std::string> kv;
            std::istringstream ss(props); std::string p;
            while (std::getline(ss, p, ',')) if (!p.empty()) kv.push_back(p);
            std::sort(kv.begin(), kv.end());
            std::string sp; for (std::size_t i = 0; i < kv.size(); ++i) { if (i) sp += ','; sp += kv[i]; }
            sortedRev[name + "\x01" + sp] = id;
        }
    }
    mc::block_rotation::Lookup lookup = [&](const std::string& name, const std::string& props) -> long {
        auto it = canonRev.find(name + "\x01" + props);
        return it == canonRev.end() ? -1 : it->second;
    };

    // FAM rows -> block short-name -> (rotFam, mirFam).
    std::unordered_map<std::string, std::pair<std::string, std::string>> fam;
    {
        std::ifstream f(famPath, std::ios::binary);
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("FAM\t", 0) != 0) continue;
            auto c = splitTab(line);
            if (c.size() < 4) continue;
            std::string key = c[1]; auto colon = key.find(':'); if (colon != std::string::npos) key = key.substr(colon + 1);
            fam[key] = { c[2], c[3] };
        }
    }

    // cases TSV: templates (NBT), cases (CASE), placed maps (PLACED).
    std::unordered_map<std::string, Tmpl> templates;
    struct Case { std::string key; int rot, mir, px, py, pz; };
    std::map<int, Case> cases;
    std::map<int, std::map<std::array<int,3>, long>> placed;    // PLACED  (knownShape=true: pure transform)
    std::map<int, std::map<std::array<int,3>, long>> placedU;   // PLACEDU (knownShape=false: + updateShape pass)
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            auto c = splitTab(line);
            if (c.empty()) continue;
            if (c[0] == "NBT" && c.size() >= 3) {
                auto root = mc::nbt::NbtReader::readGzip(b64decode(c[2]));
                if (root) templates[c[1]] = parseTemplate(*root);
            } else if (c[0] == "CASE" && c.size() >= 8) {
                cases[std::stoi(c[1])] = Case{ c[2], std::stoi(c[3]), std::stoi(c[4]), std::stoi(c[5]), std::stoi(c[6]), std::stoi(c[7]) };
            } else if (c[0] == "PLACED" && c.size() >= 6) {
                placed[std::stoi(c[1])][{std::stoi(c[2]), std::stoi(c[3]), std::stoi(c[4])}] = std::stol(c[5]);
            } else if (c[0] == "PLACEDU" && c.size() >= 6) {
                placedU[std::stoi(c[1])][{std::stoi(c[2]), std::stoi(c[3]), std::stoi(c[4])}] = std::stol(c[5]);
            }
        }
    }

    long caseChecks = 0, caseBad = 0, totalCells = 0, cellMis = 0;
    long ksFalseBad = 0, ksFalseCellMis = 0;  // C++ transform vs the knownShape=FALSE (full placement) GT
    int shown = 0;
    for (auto& [caseId, cs] : cases) {
        ++caseChecks;
        auto tit = templates.find(cs.key);
        if (tit == templates.end()) { ++caseBad; continue; }
        const Tmpl& t = tit->second;
        Rotation rot = static_cast<Rotation>(cs.rot);
        Mirror mir = static_cast<Mirror>(cs.mir);
        std::map<std::array<int,3>, long> got;
        for (const TBlock& b : t.blocks) {
            const PaletteEntry& pe = (b.stateIdx >= 0 && (std::size_t)b.stateIdx < t.palette.size())
                ? t.palette[b.stateIdx] : t.palette.empty() ? PaletteEntry{"minecraft:air",""} : t.palette[0];
            std::string shortName = pe.name.substr(pe.name.find(':') + 1);
            long baseId = -1;
            { auto it = sortedRev.find(shortName + "\x01" + pe.sortedProps); if (it != sortedRev.end()) baseId = it->second; }
            if (baseId < 0) { ++cellMis; continue; }
            // worldPos = structureTransform(localPos, mir, rot, ZERO) + position.
            BlockPos wp = structureTransform(b.pos, mir, rot, kBlockPosZero);
            std::array<int,3> wpos{ wp.x + cs.px, wp.y + cs.py, wp.z + cs.pz };
            // finalState = state.mirror(mir).rotate(rot).
            auto fi = fam.find(shortName);
            std::string rotFam = fi == fam.end() ? "?" : fi->second.first;
            std::string mirFam = fi == fam.end() ? "?" : fi->second.second;
            long mId = mc::block_rotation::mirror(states[baseId], baseId, mir, mirFam, lookup);
            if (mId < 0) { ++cellMis; continue; }
            long fId = mc::block_rotation::rotate(states[mId], mId, rot, rotFam, lookup);
            if (fId < 0) { ++cellMis; continue; }
            got[wpos] = fId;
        }
        // compare maps.
        const auto& want = placed[caseId];
        bool ok = (got.size() == want.size());
        if (ok) for (auto& [pos, id] : want) { auto g = got.find(pos); if (g == got.end() || g->second != id) { ok = false; break; } }
        totalCells += (long)want.size();
        if (!ok) {
            ++caseBad;
            for (auto& [pos, id] : want) { auto g = got.find(pos); if (g == got.end() || g->second != id) ++cellMis; }
            if (shown++ < 6)
                std::cerr << "case " << caseId << " " << cs.key << " rot=" << cs.rot << " mir=" << cs.mir
                          << " got=" << got.size() << " want=" << want.size() << "\n";
        }
        // knownShape=FALSE: the C++ transform output vs the REAL placeInWorld(knownShape=false) map
        // (which ran Block.updateFromNeighbourShapes over the placed blocks). For self-consistent
        // templates with the real tags bound, the update pass reproduces the saved connection states,
        // so this equals `got` — proving structure placement needs no separate update pass here.
        auto uit = placedU.find(caseId);
        if (uit != placedU.end()) {
            const auto& wantU = uit->second;
            bool okU = (got.size() == wantU.size());
            if (okU) for (auto& [pos, id] : wantU) { auto g = got.find(pos); if (g == got.end() || g->second != id) { okU = false; break; } }
            if (!okU) {
                ++ksFalseBad;
                for (auto& [pos, id] : wantU) { auto g = got.find(pos); if (g == got.end() || g->second != id) ++ksFalseCellMis; }
            }
        }
    }

    std::cout << "StructurePlaceInWorld cases=" << caseChecks << " casesBad=" << caseBad
              << " cells=" << totalCells << " cellMismatches=" << cellMis << "\n";
    std::cout << "  knownShape=false (vs updateShape-applied GT): casesBad=" << ksFalseBad
              << " cellMismatches=" << ksFalseCellMis << "\n";
    return (caseBad > 0 || cellMis > 0 || ksFalseBad > 0 || ksFalseCellMis > 0) ? 1 : 0;
}
