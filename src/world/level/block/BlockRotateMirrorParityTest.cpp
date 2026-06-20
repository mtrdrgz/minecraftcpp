// Parity gate for net.minecraft...BlockState.rotate(Rotation)/mirror(Mirror) over every state
// in Block.BLOCK_STATE_REGISTRY — the transforms StructureTemplate.placeInWorld applies to each
// template block (the keystone of the structure world-placement layer, #18/#19).
//
// The transform logic itself now lives in the reusable header world/level/block/BlockRotation.h
// (so the engine + the placeInWorld gate share it); this file is the certification harness: it
// loads block_states.json (-> (name,props) per id + reverse index) and the GT
// (tools/BlockRotateMirrorParity.java, R rows = real rotated/mirrored state ids, FAM rows = the
// rotate/mirror DECLARING class per block), then drives mc::block_rotation::rotate/mirror over
// every state and compares. A state is asserted iff its declaring class is in the ported set
// (others counted as TODO — never silently passed, RULE #0).
//
//   block_rotate_mirror_parity [--cases mcpp/build/block_rotate_mirror.tsv]
//                              [--states mcpp/src/assets/block_states.json]

#include "BlockRotation.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::block_rotation::StateInfo;
using mc::block_rotation::splitc;

int main(int argc, char** argv) {
    std::string casesPath = "build/block_rotate_mirror.tsv";
    std::string statesPath = "src/assets/block_states.json";
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
    mc::block_rotation::Lookup lookup = [&](const std::string& name, const std::string& props) -> long {
        auto it = revIndex.find(name + "\x01" + props);
        return it == revIndex.end() ? -1 : (long)it->second;
    };

    // 2) FAM rows -> per-block rotate/mirror declaring class. 3) R rows -> per-id expected transforms.
    std::unordered_map<std::string, std::pair<std::string, std::string>> fam;  // shortName -> (rotFam, mirFam)
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
                std::string key = c[1];
                auto colon = key.find(':');
                if (colon != std::string::npos) key = key.substr(colon + 1);
                fam[key] = { c[2], c[3] };
            }
        }
    }

    // Ported declaring classes. A state is asserted iff its block's declaring class is here.
    const std::set<std::string> rotPorted = {
        "BlockBehaviour", "HorizontalDirectionalBlock", "RotatedPillarBlock",
        "WallBlock", "CrossCollisionBlock", "RedStoneWireBlock", "MossyCarpetBlock", "VineBlock",
        "MultifaceBlock", "FlowerBedBlock", "LeafLitterBlock",
        "StairBlock", "DoorBlock", "StandingSignBlock", "CeilingHangingSignBlock",
        "WallSignBlock", "WallHangingSignBlock", "ShelfBlock", "ChiseledBookShelfBlock",
        "BannerBlock", "SkullBlock", "WallBannerBlock", "WallSkullBlock", "BaseCoralWallFanBlock",
        "WallTorchBlock", "RedstoneWallTorchBlock", "AmethystClusterBlock", "CopperGolemStatueBlock",
        "RodBlock", "LadderBlock", "BellBlock", "GrindstoneBlock", "AttachedStemBlock", "AnvilBlock",
        "HugeMushroomBlock",
        "ChestBlock", "EnderChestBlock", "BarrelBlock", "AbstractFurnaceBlock", "DispenserBlock",
        "ObserverBlock", "HopperBlock", "BeehiveBlock", "CampfireBlock", "LecternBlock",
        "StonecutterBlock", "SmallDripleafBlock", "DecoratedPotBlock", "VaultBlock", "CommandBlock",
        "ShulkerBoxBlock", "CalibratedSculkSensorBlock", "EndPortalFrameBlock", "JigsawBlock",
        "CrafterBlock", "CreakingHeartBlock",
        "RailBlock", "PoweredRailBlock", "DetectorRailBlock", "PistonBaseBlock", "PistonHeadBlock",
        "MovingPistonBlock", "InfestedRotatedPillarBlock", "NetherPortalBlock", "TripWireBlock",
        "TripWireHookBlock" };
    // AnvilBlock/CreakingHeartBlock/InfestedRotatedPillarBlock/NetherPortalBlock declare only rotate;
    // their mirror is the inherited BlockBehaviour identity (covered by "BlockBehaviour").
    const std::set<std::string> mirPorted = {
        "BlockBehaviour", "HorizontalDirectionalBlock", "RotatedPillarBlock",
        "WallBlock", "CrossCollisionBlock", "RedStoneWireBlock", "MossyCarpetBlock", "VineBlock",
        "MultifaceBlock", "FlowerBedBlock", "LeafLitterBlock",
        "StairBlock", "DoorBlock", "StandingSignBlock", "CeilingHangingSignBlock",
        "WallSignBlock", "WallHangingSignBlock", "ShelfBlock", "ChiseledBookShelfBlock",
        "BannerBlock", "SkullBlock", "WallBannerBlock", "WallSkullBlock", "BaseCoralWallFanBlock",
        "WallTorchBlock", "RedstoneWallTorchBlock", "AmethystClusterBlock", "CopperGolemStatueBlock",
        "RodBlock", "LadderBlock", "BellBlock", "GrindstoneBlock", "AttachedStemBlock",
        "HugeMushroomBlock",
        "ChestBlock", "EnderChestBlock", "BarrelBlock", "AbstractFurnaceBlock", "DispenserBlock",
        "ObserverBlock", "HopperBlock", "BeehiveBlock", "CampfireBlock", "LecternBlock",
        "StonecutterBlock", "SmallDripleafBlock", "DecoratedPotBlock", "VaultBlock", "CommandBlock",
        "ShulkerBoxBlock", "CalibratedSculkSensorBlock", "EndPortalFrameBlock", "JigsawBlock",
        "CrafterBlock",
        "RailBlock", "PoweredRailBlock", "DetectorRailBlock", "PistonBaseBlock", "PistonHeadBlock",
        "MovingPistonBlock", "TripWireBlock", "TripWireHookBlock" };

    long rotCert = 0, rotMis = 0, rotTodo = 0, mirCert = 0, mirMis = 0, mirTodo = 0;
    std::map<std::string, long> todoFamStates;
    int shown = 0;
    for (uint32_t id = 0; id < states.size(); ++id) {
        const Exp& e = expected[id];
        if (e.cw90 < 0) continue;  // no R row (shouldn't happen)
        auto fi = fam.find(states[id].name);
        std::string rotFam = fi == fam.end() ? "?" : fi->second.first;
        std::string mirFam = fi == fam.end() ? "?" : fi->second.second;

        if (rotPorted.count(rotFam)) {
            long g90 = mc::block_rotation::rotate(states[id], (long)id, Rotation::CLOCKWISE_90, rotFam, lookup);
            long g180 = mc::block_rotation::rotate(states[id], (long)id, Rotation::CLOCKWISE_180, rotFam, lookup);
            long g270 = mc::block_rotation::rotate(states[id], (long)id, Rotation::COUNTERCLOCKWISE_90, rotFam, lookup);
            for (auto [got, want, lbl] : { std::tuple<long,long,const char*>{g90, e.cw90, "cw90"},
                                           {g180, e.cw180, "cw180"}, {g270, e.ccw90, "ccw90"} }) {
                if (got == want) ++rotCert;
                else { ++rotMis;
                    if (shown++ < 16) std::cerr << "ROT mismatch id=" << id << " " << states[id].name
                        << "[" << states[id].props << "] " << lbl << " fam=" << rotFam
                        << " got=" << got << " want=" << want << "\n"; }
            }
        } else { rotTodo += 3; todoFamStates[rotFam] += 3; }

        if (mirPorted.count(mirFam)) {
            long glr = mc::block_rotation::mirror(states[id], (long)id, Mirror::LEFT_RIGHT, mirFam, lookup);
            long gfb = mc::block_rotation::mirror(states[id], (long)id, Mirror::FRONT_BACK, mirFam, lookup);
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
    std::vector<std::pair<std::string,long>> todo(todoFamStates.begin(), todoFamStates.end());
    std::sort(todo.begin(), todo.end(), [](auto& a, auto& b){ return a.second > b.second; });
    std::cout << "remaining rotate families (states): ";
    for (std::size_t i = 0; i < todo.size() && i < 12; ++i)
        std::cout << todo[i].first << "=" << todo[i].second << " ";
    std::cout << "\n";

    return (rotMis > 0 || mirMis > 0) ? 1 : 0;
}
