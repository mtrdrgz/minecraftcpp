// Parity gate for net.minecraft...BlockState.rotate(Rotation) / .mirror(Mirror) — the
// per-block-state transforms StructureTemplate.placeInWorld applies to every template
// block (the keystone of the structure world-placement layer, #18/#19).
//
// Ground truth: tools/BlockRotateMirrorParity.java drives the REAL BlockState.rotate/
// mirror over every state in Block.BLOCK_STATE_REGISTRY (R rows: rotated/mirrored state
// ids) and tags each block with the class that DECLARES its rotate/mirror (FAM rows).
//
// The port reproduces a transform by (a) reading the state's (block, canonical-props)
// from the engine's block_states.json, (b) transforming the relevant property values
// per the block's declaring-class logic (reusing the certified StructureTransforms
// Rotation.rotate(Direction) / Mirror.getRotation(Direction)), (c) re-serialising the
// canonical props and looking the result up in a (block,props)->id reverse index.
//
// Certification grows FAMILY-BY-FAMILY: a state is asserted iff its block's declaring
// class is in the ported set below; others are counted as TODO (NOT silently passed —
// RULE #0). This run certifies the three highest-coverage rotate families
// (no-op / HorizontalDirectionalBlock FACING / RotatedPillarBlock AXIS) byte-exact; the
// remaining families are an explicit worklist (printed) for follow-up.
//
//   block_rotate_mirror_parity [--cases mcpp/build/block_rotate_mirror.tsv]
//                              [--states mcpp/src/assets/block_states.json]

#include "../levelgen/structure/templatesystem/StructureTransforms.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using mc::Direction;
using mc::Axis;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::rotationRotate;
using mc::levelgen::structure::mirrorGetRotation;

namespace {

std::vector<std::string> splitc(const std::string& s, char d) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, d)) o.push_back(c);
    return o;
}

// Direction <-> serialized name (Direction.getName()).
Direction dirByName(const std::string& n) {
    if (n == "down") return Direction::DOWN;
    if (n == "up") return Direction::UP;
    if (n == "north") return Direction::NORTH;
    if (n == "south") return Direction::SOUTH;
    if (n == "west") return Direction::WEST;
    return Direction::EAST; // "east"
}
const char* dirName(Direction d) {
    switch (d) {
        case Direction::DOWN: return "down";
        case Direction::UP: return "up";
        case Direction::NORTH: return "north";
        case Direction::SOUTH: return "south";
        case Direction::WEST: return "west";
        default: return "east";
    }
}

// Per-state info parsed from block_states.json.
struct StateInfo { std::string name; std::string props; };

// Replace the value of property `key` in a canonical props string, preserving the exact
// key order/formatting (we only ever substitute a value, never add/remove keys).
// Returns false if the key is absent (a port bug for the families we claim to handle).
bool replaceProp(const std::string& props, const std::string& key,
                 const std::string& newVal, std::string& out) {
    auto pairs = splitc(props, ',');
    bool found = false;
    for (auto& p : pairs) {
        auto eq = p.find('=');
        if (eq == std::string::npos) continue;
        if (p.substr(0, eq) == key) { p = key + "=" + newVal; found = true; }
    }
    if (!found) return false;
    out.clear();
    for (std::size_t i = 0; i < pairs.size(); ++i) { if (i) out += ','; out += pairs[i]; }
    return true;
}
std::string getProp(const std::string& props, const std::string& key) {
    for (auto& p : splitc(props, ',')) {
        auto eq = p.find('=');
        if (eq != std::string::npos && p.substr(0, eq) == key) return p.substr(eq + 1);
    }
    return "";
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/block_rotate_mirror.tsv";
    std::string statesPath = "mcpp/src/assets/block_states.json";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
    }

    // 1) block_states.json -> per-id (name, props) + (name|props)->id reverse index.
    std::vector<StateInfo> states;
    std::unordered_map<std::string, uint32_t> revIndex;
    {
        std::ifstream f(statesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
        nlohmann::json j; f >> j;
        auto arr = j.at("states");
        states.resize(arr.size());
        for (auto& s : arr) {
            uint32_t id = s.at("id").get<uint32_t>();
            StateInfo si{ s.at("name").get<std::string>(), s.value("props", std::string()) };
            states[id] = si;
            revIndex[si.name + "\x01" + si.props] = id;
        }
    }
    auto lookup = [&](const std::string& name, const std::string& props) -> long {
        auto it = revIndex.find(name + "\x01" + props);
        return it == revIndex.end() ? -1 : (long)it->second;
    };

    // 2) FAM rows -> per-block rotate/mirror declaring class.
    std::unordered_map<std::string, std::pair<std::string, std::string>> fam;  // shortName -> (rotFam, mirFam)
    // 3) R rows -> per-id expected transforms.
    struct Exp { long cw90, cw180, ccw90, mlr, mfb; };
    std::vector<Exp> expected(states.size(), Exp{-1, -1, -1, -1, -1});
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();  // TSV is CRLF (Out-File)
            auto c = splitc(line, '\t');
            if (c.empty()) continue;
            if (c[0] == "R" && c.size() >= 7) {
                uint32_t id = (uint32_t)std::stoul(c[1]);
                if (id < expected.size())
                    expected[id] = Exp{ std::stol(c[2]), std::stol(c[3]), std::stol(c[4]),
                                        std::stol(c[5]), std::stol(c[6]) };
            } else if (c[0] == "FAM" && c.size() >= 4) {
                // c[1] is "minecraft:foo"; states[].name is the short key without namespace.
                std::string key = c[1];
                auto colon = key.find(':');
                if (colon != std::string::npos) key = key.substr(colon + 1);
                fam[key] = { c[2], c[3] };
            }
        }
    }

    // Ported declaring classes. A state is asserted iff its block's declaring class is here.
    const std::set<std::string> rotPorted = { "BlockBehaviour", "HorizontalDirectionalBlock", "RotatedPillarBlock" };
    const std::set<std::string> mirPorted = { "BlockBehaviour", "HorizontalDirectionalBlock", "RotatedPillarBlock" };

    // rotate(stateId, rotation) for a ported declaring class -> new state id (or -1).
    auto doRotate = [&](uint32_t id, Rotation rot, const std::string& f) -> long {
        const StateInfo& s = states[id];
        if (f == "BlockBehaviour") return id;                       // default: unchanged
        if (f == "HorizontalDirectionalBlock") {                    // setValue(FACING, rotation.rotate(FACING))
            Direction nf = rotationRotate(rot, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        if (f == "RotatedPillarBlock") {                            // rotatePillar: 90deg -> swap X<->Z axis
            if (rot == Rotation::CLOCKWISE_90 || rot == Rotation::COUNTERCLOCKWISE_90) {
                std::string ax = getProp(s.props, "axis");
                std::string na = ax == "x" ? "z" : (ax == "z" ? "x" : ax);
                std::string np;
                if (!replaceProp(s.props, "axis", na, np)) return -1;
                return lookup(s.name, np);
            }
            return id;                                              // 180 / none: unchanged
        }
        return -2;
    };
    // mirror(stateId, mirror) for a ported declaring class -> new state id (or -1).
    auto doMirror = [&](uint32_t id, Mirror mir, const std::string& f) -> long {
        const StateInfo& s = states[id];
        if (f == "BlockBehaviour") return id;                       // default + RotatedPillarBlock (no override)
        if (f == "HorizontalDirectionalBlock") {                    // rotate(mirror.getRotation(FACING))
            Direction facing = dirByName(getProp(s.props, "facing"));
            Rotation r = mirrorGetRotation(mir, facing);
            return doRotate(id, r, "HorizontalDirectionalBlock");
        }
        return -2;
    };

    long rotCert = 0, rotMis = 0, rotTodo = 0, mirCert = 0, mirMis = 0, mirTodo = 0;
    std::map<std::string, long> todoFamStates;   // declaringClass -> #states deferred (rotate)
    int shown = 0;
    for (uint32_t id = 0; id < states.size(); ++id) {
        const Exp& e = expected[id];
        if (e.cw90 < 0) continue;  // no R row (shouldn't happen)
        auto fi = fam.find(states[id].name);
        std::string rotFam = fi == fam.end() ? "?" : fi->second.first;
        std::string mirFam = fi == fam.end() ? "?" : fi->second.second;

        // rotate (3 non-identity rotations).
        if (rotPorted.count(rotFam)) {
            long g90 = doRotate(id, Rotation::CLOCKWISE_90, rotFam);
            long g180 = doRotate(id, Rotation::CLOCKWISE_180, rotFam);
            long g270 = doRotate(id, Rotation::COUNTERCLOCKWISE_90, rotFam);
            for (auto [got, want, lbl] : { std::tuple<long,long,const char*>{g90, e.cw90, "cw90"},
                                           {g180, e.cw180, "cw180"}, {g270, e.ccw90, "ccw90"} }) {
                if (got == want) ++rotCert;
                else { ++rotMis;
                    if (shown++ < 16) std::cerr << "ROT mismatch id=" << id << " " << states[id].name
                        << "[" << states[id].props << "] " << lbl << " fam=" << rotFam
                        << " got=" << got << " want=" << want << "\n"; }
            }
        } else { rotTodo += 3; todoFamStates[rotFam] += 3; }

        // mirror (2 non-identity mirrors).
        if (mirPorted.count(mirFam)) {
            long glr = doMirror(id, Mirror::LEFT_RIGHT, mirFam);
            long gfb = doMirror(id, Mirror::FRONT_BACK, mirFam);
            for (auto [got, want, lbl] : { std::tuple<long,long,const char*>{glr, e.mlr, "lr"},
                                           {gfb, e.mfb, "fb"} }) {
                if (got == want) ++mirCert;
                else { ++mirMis;
                    if (shown++ < 16) std::cerr << "MIR mismatch id=" << id << " " << states[id].name
                        << "[" << states[id].props << "] " << lbl << " fam=" << mirFam
                        << " got=" << got << " want=" << want << "\n"; }
            }
        } else { mirTodo += 2; }
    }

    std::cout << "BlockRotateMirror rotate: certified=" << rotCert << " mismatches=" << rotMis
              << " todo=" << rotTodo << " | mirror: certified=" << mirCert << " mismatches=" << mirMis
              << " todo=" << mirTodo << "\n";
    // top remaining rotate families (the fan-out worklist).
    std::vector<std::pair<std::string,long>> todo(todoFamStates.begin(), todoFamStates.end());
    std::sort(todo.begin(), todo.end(), [](auto& a, auto& b){ return a.second > b.second; });
    std::cout << "remaining rotate families (states): ";
    for (std::size_t i = 0; i < todo.size() && i < 12; ++i)
        std::cout << todo[i].first << "=" << todo[i].second << " ";
    std::cout << "\n";

    return (rotMis > 0 || mirMis > 0) ? 1 : 0;
}
