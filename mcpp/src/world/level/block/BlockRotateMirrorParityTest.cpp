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
using mc::levelgen::structure::rotationRotateInt;
using mc::levelgen::structure::mirrorGetRotation;
using mc::levelgen::structure::mirrorMirror;
using mc::levelgen::structure::mirrorMirrorInt;
using mc::directionAxis;

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

// Permute the four horizontal directional sub-properties {north,east,south,west} of a
// CrossCollisionBlock-style state by a Rotation — the SHARED rotate idiom of WallBlock/
// CrossCollisionBlock/RedStoneWireBlock/MossyCarpetBlock/VineBlock (new.<dir> = old.<rotate(dir)>).
// Value type (WallSide/RedstoneSide/bool) is irrelevant to the string-level permutation.
bool permuteNESW(const std::string& props, Rotation rot, std::string& out) {
    std::string n = getProp(props, "north"), e = getProp(props, "east");
    std::string s = getProp(props, "south"), w = getProp(props, "west");
    if (n.empty() || e.empty() || s.empty() || w.empty()) return false;
    std::string nn, ne, ns, nw;
    switch (rot) {
        case Rotation::CLOCKWISE_180:       nn = s; ne = w; ns = n; nw = e; break;
        case Rotation::COUNTERCLOCKWISE_90: nn = e; ne = s; ns = w; nw = n; break;
        case Rotation::CLOCKWISE_90:        nn = w; ne = n; ns = e; nw = s; break;
        default:                            nn = n; ne = e; ns = s; nw = w; break;  // NONE
    }
    std::string t;
    if (!replaceProp(props, "north", nn, t)) return false; out = t;
    if (!replaceProp(out,   "east",  ne, t)) return false; out = t;
    if (!replaceProp(out,   "south", ns, t)) return false; out = t;
    if (!replaceProp(out,   "west",  nw, t)) return false; out = t;
    return true;
}
// Mirror {north,east,south,west}: LEFT_RIGHT swaps N<->S, FRONT_BACK swaps E<->W (NONE = identity).
bool mirrorNESW(const std::string& props, Mirror mir, std::string& out) {
    std::string n = getProp(props, "north"), e = getProp(props, "east");
    std::string s = getProp(props, "south"), w = getProp(props, "west");
    if (n.empty() || e.empty() || s.empty() || w.empty()) return false;
    std::string t = props;
    if (mir == Mirror::LEFT_RIGHT) {
        if (!replaceProp(t, "north", s, out)) return false; t = out;
        if (!replaceProp(t, "south", n, out)) return false; t = out;
    } else if (mir == Mirror::FRONT_BACK) {
        if (!replaceProp(t, "east", w, out)) return false; t = out;
        if (!replaceProp(t, "west", e, out)) return false; t = out;
    } else {
        out = props;
    }
    return true;
}
// MultifaceBlock.mapDirections over the SIX face keys: new[map(dir)] = old[dir]; non-face
// props (waterlogged) untouched. No MultifaceBlock overrides isFaceSupported, so the map
// always applies. rotate/mirror keep Y (up/down) fixed; only the 4 horizontals move.
bool mapMultifaceRotate(const std::string& props, Rotation rot, std::string& out) {
    static const Direction FACES[6] = { Direction::DOWN, Direction::UP, Direction::NORTH,
                                        Direction::SOUTH, Direction::WEST, Direction::EAST };
    std::string oldVal[6];
    for (int i = 0; i < 6; ++i) { oldVal[i] = getProp(props, dirName(FACES[i])); if (oldVal[i].empty()) return false; }
    out = props; std::string t;
    for (int i = 0; i < 6; ++i) {
        if (!replaceProp(out, dirName(rotationRotate(rot, FACES[i])), oldVal[i], t)) return false;
        out = t;
    }
    return true;
}
bool mapMultifaceMirror(const std::string& props, Mirror mir, std::string& out) {
    static const Direction FACES[6] = { Direction::DOWN, Direction::UP, Direction::NORTH,
                                        Direction::SOUTH, Direction::WEST, Direction::EAST };
    std::string oldVal[6];
    for (int i = 0; i < 6; ++i) { oldVal[i] = getProp(props, dirName(FACES[i])); if (oldVal[i].empty()) return false; }
    out = props; std::string t;
    for (int i = 0; i < 6; ++i) {
        if (!replaceProp(out, dirName(mirrorMirror(mir, FACES[i])), oldVal[i], t)) return false;
        out = t;
    }
    return true;
}
// HugeMushroomBlock.rotate/mirror permute the 6 directional booleans:
//   result[transform(d)] = old[d] for d in the 6 faces (a bijection, so order-independent).
template <typename TransformDir>
bool permuteFaceBools(const std::string& props, TransformDir xf, std::string& out) {
    static const char* kDirs[6] = { "north", "south", "east", "west", "up", "down" };
    std::map<std::string, std::string> dest;   // dest dir-name -> source bool value
    for (const char* d : kDirs) {
        std::string v = getProp(props, d);
        if (v.empty()) return false;
        dest[dirName(xf(dirByName(d)))] = v;
    }
    out = props;
    for (const auto& [dir, val] : dest) {
        std::string tmp;
        if (!replaceProp(out, dir, val, tmp)) return false;
        out = tmp;
    }
    return true;
}

// BaseRailBlock.rotate(RailShape,Rotation) — BaseRailBlock.java:151-233. Shared by
// RailBlock/PoweredRailBlock/DetectorRailBlock (each delegates to this inherited switch).
std::string railRotateShape(const std::string& shape, Rotation rot) {
    switch (rot) {
        case Rotation::CLOCKWISE_90:
            if (shape == "ascending_east")  return "ascending_south";
            if (shape == "ascending_west")  return "ascending_north";
            if (shape == "ascending_north") return "ascending_east";
            if (shape == "ascending_south") return "ascending_west";
            if (shape == "north_south")     return "east_west";
            if (shape == "east_west")       return "north_south";
            if (shape == "south_east")      return "south_west";
            if (shape == "south_west")      return "north_west";
            if (shape == "north_west")      return "north_east";
            if (shape == "north_east")      return "south_east";
            return shape;
        case Rotation::CLOCKWISE_180:
            if (shape == "ascending_east")  return "ascending_west";
            if (shape == "ascending_west")  return "ascending_east";
            if (shape == "ascending_north") return "ascending_south";
            if (shape == "ascending_south") return "ascending_north";
            if (shape == "south_east")      return "north_west";
            if (shape == "south_west")      return "north_east";
            if (shape == "north_west")      return "south_east";
            if (shape == "north_east")      return "south_west";
            return shape;
        case Rotation::COUNTERCLOCKWISE_90:
            if (shape == "ascending_east")  return "ascending_north";
            if (shape == "ascending_west")  return "ascending_south";
            if (shape == "ascending_north") return "ascending_west";
            if (shape == "ascending_south") return "ascending_east";
            if (shape == "north_south")     return "east_west";
            if (shape == "east_west")       return "north_south";
            if (shape == "south_east")      return "north_east";
            if (shape == "south_west")      return "south_east";
            if (shape == "north_west")      return "south_west";
            if (shape == "north_east")      return "north_west";
            return shape;
        default:
            return shape;  // NONE
    }
}
// BaseRailBlock.mirror(RailShape,Mirror) — BaseRailBlock.java:235-281.
std::string railMirrorShape(const std::string& shape, Mirror mir) {
    switch (mir) {
        case Mirror::LEFT_RIGHT:
            if (shape == "ascending_north") return "ascending_south";
            if (shape == "ascending_south") return "ascending_north";
            if (shape == "south_east")      return "north_east";
            if (shape == "south_west")      return "north_west";
            if (shape == "north_west")      return "south_west";
            if (shape == "north_east")      return "south_east";
            return shape;
        case Mirror::FRONT_BACK:
            if (shape == "ascending_east")  return "ascending_west";
            if (shape == "ascending_west")  return "ascending_east";
            if (shape == "south_east")      return "south_west";
            if (shape == "south_west")      return "south_east";
            if (shape == "north_west")      return "north_east";
            if (shape == "north_east")      return "north_west";
            return shape;
        default:
            return shape;  // NONE
    }
}

// JigsawBlock/CrafterBlock ORIENTATION = FrontAndTop ("front_top", e.g. "down_east").
// rotate = ORIENTATION.rotate(rotation.rotation()) ; mirror = ORIENTATION.rotate(mirror.rotation())
// = fromFrontAndTop(g.rotate(front), g.rotate(top)). VERIFIED over all 6 dirs that the
// OctahedralGroup rotations equal the certified primitives (ROT_90_Y_NEG=rotationRotate(CW90),
// ROT_180_FACE_XZ=CW180, ROT_90_Y_POS=CCW90, INVERT_Z=mirrorMirror(LEFT_RIGHT),
// INVERT_X=mirrorMirror(FRONT_BACK)), so we reuse them on both front and top.
template <typename TransformDir>
bool transformOrientation(const std::string& props, TransformDir xf, std::string& out) {
    std::string ori = getProp(props, "orientation");
    if (ori.empty()) return false;
    auto us = ori.find('_');
    if (us == std::string::npos) return false;
    Direction front = dirByName(ori.substr(0, us));
    Direction top   = dirByName(ori.substr(us + 1));
    std::string nv = std::string(dirName(xf(front))) + "_" + dirName(xf(top));
    return replaceProp(props, "orientation", nv, out);
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
        // tail (agents A+B): FACING/ORIENTATION + axis + rails/pistons/tripwire
        "ChestBlock", "EnderChestBlock", "BarrelBlock", "AbstractFurnaceBlock", "DispenserBlock",
        "ObserverBlock", "HopperBlock", "BeehiveBlock", "CampfireBlock", "LecternBlock",
        "StonecutterBlock", "SmallDripleafBlock", "DecoratedPotBlock", "VaultBlock", "CommandBlock",
        "ShulkerBoxBlock", "CalibratedSculkSensorBlock", "EndPortalFrameBlock", "JigsawBlock",
        "CrafterBlock", "CreakingHeartBlock",
        "RailBlock", "PoweredRailBlock", "DetectorRailBlock", "PistonBaseBlock", "PistonHeadBlock",
        "MovingPistonBlock", "InfestedRotatedPillarBlock", "NetherPortalBlock", "TripWireBlock",
        "TripWireHookBlock" };
    // AnvilBlock/CreakingHeartBlock/InfestedRotatedPillarBlock/NetherPortalBlock declare only rotate;
    // their mirror is the inherited BlockBehaviour identity (already covered by "BlockBehaviour").
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
        // tail (agents A+B): FACING/ORIENTATION + rails/pistons/tripwire (NOT the no-mirror ones)
        "ChestBlock", "EnderChestBlock", "BarrelBlock", "AbstractFurnaceBlock", "DispenserBlock",
        "ObserverBlock", "HopperBlock", "BeehiveBlock", "CampfireBlock", "LecternBlock",
        "StonecutterBlock", "SmallDripleafBlock", "DecoratedPotBlock", "VaultBlock", "CommandBlock",
        "ShulkerBoxBlock", "CalibratedSculkSensorBlock", "EndPortalFrameBlock", "JigsawBlock",
        "CrafterBlock",
        "RailBlock", "PoweredRailBlock", "DetectorRailBlock", "PistonBaseBlock", "PistonHeadBlock",
        "MovingPistonBlock", "TripWireBlock", "TripWireHookBlock" };

    // rotate(stateId, rotation) for a ported declaring class -> new state id (or -1).
    auto doRotate = [&](uint32_t id, Rotation rot, const std::string& f) -> long {
        const StateInfo& s = states[id];
        if (f == "BlockBehaviour") return id;                       // default: unchanged
        // setValue(FACING, rotation.rotate(FACING)) — HorizontalDirectionalBlock + every own-class
        // declarer that uses the same FACING idiom (4-dir, 6-dir, FACING_HOPPER all handled, since
        // rotationRotate leaves the Y axis fixed). Key is always "facing".
        if (f == "HorizontalDirectionalBlock" ||
            f == "ChestBlock" || f == "EnderChestBlock" || f == "BarrelBlock" ||
            f == "AbstractFurnaceBlock" || f == "DispenserBlock" || f == "ObserverBlock" ||
            f == "HopperBlock" || f == "BeehiveBlock" || f == "CampfireBlock" ||
            f == "LecternBlock" || f == "StonecutterBlock" || f == "SmallDripleafBlock" ||
            f == "DecoratedPotBlock" || f == "VaultBlock" || f == "CommandBlock" ||
            f == "ShulkerBoxBlock" || f == "CalibratedSculkSensorBlock" || f == "EndPortalFrameBlock") {
            Direction nf = rotationRotate(rot, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // JigsawBlock/CrafterBlock — ORIENTATION (FrontAndTop) by rotation.rotation().
        if (f == "JigsawBlock" || f == "CrafterBlock") {
            std::string np;
            if (!transformOrientation(s.props, [&](Direction d){ return rotationRotate(rot, d); }, np)) return -1;
            return lookup(s.name, np);
        }
        if (f == "RotatedPillarBlock" || f == "CreakingHeartBlock") {  // rotatePillar: 90deg -> swap X<->Z axis
            if (rot == Rotation::CLOCKWISE_90 || rot == Rotation::COUNTERCLOCKWISE_90) {
                std::string ax = getProp(s.props, "axis");
                std::string na = ax == "x" ? "z" : (ax == "z" ? "x" : ax);
                std::string np;
                if (!replaceProp(s.props, "axis", na, np)) return -1;
                return lookup(s.name, np);
            }
            return id;                                              // 180 / none: unchanged
        }
        // --- StairBlock: rotate sets only FACING. StairBlock.java:168-171 ---
        // --- DoorBlock: rotate sets only FACING. DoorBlock.java:251-254 ---
        // --- WallSignBlock/WallHangingSignBlock/ShelfBlock/ChiseledBookShelfBlock: FACING only ---
        if (f == "StairBlock" || f == "DoorBlock" || f == "WallSignBlock" ||
            f == "WallHangingSignBlock" || f == "ShelfBlock" || f == "ChiseledBookShelfBlock") {
            Direction nf = rotationRotate(rot, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // --- StandingSignBlock/CeilingHangingSignBlock: rotation ROTATION_16 via Rotation.rotate(int,16).
        //     StandingSignBlock.java:71-74, CeilingHangingSignBlock.java:158-161 ---
        if (f == "StandingSignBlock" || f == "CeilingHangingSignBlock") {
            int v = std::stoi(getProp(s.props, "rotation"));
            std::string np;
            if (!replaceProp(s.props, "rotation", std::to_string(rotationRotateInt(rot, v, 16)), np)) return -1;
            return lookup(s.name, np);
        }
        // WallBlock/CrossCollisionBlock/RedStoneWireBlock/MossyCarpetBlock/VineBlock — {N,E,S,W} permute.
        if (f == "WallBlock" || f == "CrossCollisionBlock" || f == "RedStoneWireBlock" ||
            f == "MossyCarpetBlock" || f == "VineBlock") {
            std::string np; if (!permuteNESW(s.props, rot, np)) return -1; return lookup(s.name, np);
        }
        if (f == "MultifaceBlock") {                 // mapDirections by rotation.rotate
            std::string np; if (!mapMultifaceRotate(s.props, rot, np)) return -1; return lookup(s.name, np);
        }
        // BannerBlock/SkullBlock — ROTATION_16 via Rotation.rotate(int,16).
        if (f == "BannerBlock" || f == "SkullBlock") {
            int v = std::stoi(getProp(s.props, "rotation"));
            std::string np;
            if (!replaceProp(s.props, "rotation", std::to_string(rotationRotateInt(rot, v, 16)), np)) return -1;
            return lookup(s.name, np);
        }
        // FACING-only rotate: setValue(FACING, rotation.rotate(FACING)). (Y axis unchanged -> ok for 6-dir.)
        if (f == "FlowerBedBlock" || f == "LeafLitterBlock" || f == "WallBannerBlock" ||
            f == "WallSkullBlock" || f == "BaseCoralWallFanBlock" || f == "WallTorchBlock" ||
            f == "RedstoneWallTorchBlock" || f == "AmethystClusterBlock" || f == "CopperGolemStatueBlock" ||
            f == "RodBlock" || f == "LadderBlock" || f == "BellBlock" || f == "GrindstoneBlock" ||
            f == "AttachedStemBlock" || f == "AnvilBlock") {
            Direction nf = rotationRotate(rot, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // HugeMushroomBlock — permute 6 face bools by rotation.rotate(Direction).
        if (f == "HugeMushroomBlock") {
            std::string np;
            if (!permuteFaceBools(s.props, [&](Direction d){ return rotationRotate(rot, d); }, np)) return -1;
            return lookup(s.name, np);
        }
        // Rails: rotate RAIL_SHAPE via BaseRailBlock switch. RailBlock/PoweredRailBlock/DetectorRailBlock.
        if (f == "RailBlock" || f == "PoweredRailBlock" || f == "DetectorRailBlock") {
            std::string np;
            if (!replaceProp(s.props, "shape", railRotateShape(getProp(s.props, "shape"), rot), np)) return -1;
            return lookup(s.name, np);
        }
        // Pistons + TripWireHook: setValue(FACING, rotation.rotate(FACING)) (6-dir for pistons).
        if (f == "PistonBaseBlock" || f == "PistonHeadBlock" || f == "MovingPistonBlock" ||
            f == "TripWireHookBlock") {
            Direction nf = rotationRotate(rot, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // InfestedRotatedPillarBlock / NetherPortalBlock: 90deg swaps horizontal AXIS x<->z; else unchanged.
        if (f == "InfestedRotatedPillarBlock" || f == "NetherPortalBlock") {
            if (rot == Rotation::CLOCKWISE_90 || rot == Rotation::COUNTERCLOCKWISE_90) {
                std::string ax = getProp(s.props, "axis");
                std::string na = ax == "x" ? "z" : (ax == "z" ? "x" : ax);
                std::string np;
                if (!replaceProp(s.props, "axis", na, np)) return -1;
                return lookup(s.name, np);
            }
            return id;
        }
        // TripWireBlock: NSEW bool permute (same as CrossCollisionBlock).
        if (f == "TripWireBlock") {
            std::string np; if (!permuteNESW(s.props, rot, np)) return -1; return lookup(s.name, np);
        }
        return -2;
    };
    // mirror(stateId, mirror) for a ported declaring class -> new state id (or -1).
    auto doMirror = [&](uint32_t id, Mirror mir, const std::string& f) -> long {
        const StateInfo& s = states[id];
        if (f == "BlockBehaviour") return id;                       // default + RotatedPillarBlock (no override)
        // mirror = state.rotate(mirror.getRotation(FACING)) — HorizontalDirectionalBlock + the same
        // own-class FACING declarers as in doRotate (routes back to the FACING rotate logic).
        if (f == "HorizontalDirectionalBlock" ||
            f == "ChestBlock" || f == "EnderChestBlock" || f == "BarrelBlock" ||
            f == "AbstractFurnaceBlock" || f == "DispenserBlock" || f == "ObserverBlock" ||
            f == "HopperBlock" || f == "BeehiveBlock" || f == "CampfireBlock" ||
            f == "LecternBlock" || f == "StonecutterBlock" || f == "SmallDripleafBlock" ||
            f == "DecoratedPotBlock" || f == "VaultBlock" || f == "CommandBlock" ||
            f == "ShulkerBoxBlock" || f == "CalibratedSculkSensorBlock" || f == "EndPortalFrameBlock") {
            Direction facing = dirByName(getProp(s.props, "facing"));
            Rotation r = mirrorGetRotation(mir, facing);
            return doRotate(id, r, "HorizontalDirectionalBlock");
        }
        // JigsawBlock/CrafterBlock — ORIENTATION (FrontAndTop) by mirror.rotation() (= mirrorMirror,
        // NOT getRotation — mirror is applied as the inversion group on both front and top).
        if (f == "JigsawBlock" || f == "CrafterBlock") {
            std::string np;
            if (!transformOrientation(s.props, [&](Direction d){ return mirrorMirror(mir, d); }, np)) return -1;
            return lookup(s.name, np);
        }
        // --- StairBlock mirror depends on SHAPE. StairBlock.java:173-212 ---
        if (f == "StairBlock") {
            Axis ax = directionAxis(dirByName(getProp(s.props, "facing")));
            std::string shape = getProp(s.props, "shape");
            // state.rotate(CLOCKWISE_180): StairBlock.rotate only touches FACING.
            Direction nf = rotationRotate(Rotation::CLOCKWISE_180, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            std::string newShape;
            if (mir == Mirror::LEFT_RIGHT && ax == Axis::Z) {
                if (shape == "outer_left") newShape = "outer_right";
                else if (shape == "inner_right") newShape = "inner_left";
                else if (shape == "inner_left") newShape = "inner_right";
                else if (shape == "outer_right") newShape = "outer_left";
                // straight: no SHAPE change.
            } else if (mir == Mirror::FRONT_BACK && ax == Axis::X) {
                if (shape == "outer_left") newShape = "outer_right";
                else if (shape == "outer_right") newShape = "outer_left";
                // straight / inner_left / inner_right: no SHAPE change.
            } else {
                return id;  // axis mismatch -> super.mirror (BlockBehaviour) = unchanged
            }
            if (!newShape.empty()) {
                std::string np2;
                if (!replaceProp(np, "shape", newShape, np2)) return -1;
                np = np2;
            }
            return lookup(s.name, np);
        }
        // --- DoorBlock: NONE->unchanged; else rotate(mirror.getRotation(FACING)) then cycle(HINGE).
        //     DoorBlock.java:256-259 ---
        if (f == "DoorBlock") {
            if (mir == Mirror::NONE) return id;
            Direction facing = dirByName(getProp(s.props, "facing"));
            Direction nf = rotationRotate(mirrorGetRotation(mir, facing), facing);
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            std::string hinge = getProp(np, "hinge");
            std::string np2;
            if (!replaceProp(np, "hinge", hinge == "left" ? "right" : "left", np2)) return -1;
            return lookup(s.name, np2);
        }
        // --- WallSignBlock/WallHangingSignBlock/ShelfBlock/ChiseledBookShelfBlock:
        //     mirror = rotate(mirror.getRotation(FACING)) — FACING only ---
        if (f == "WallSignBlock" || f == "WallHangingSignBlock" ||
            f == "ShelfBlock" || f == "ChiseledBookShelfBlock") {
            Direction facing = dirByName(getProp(s.props, "facing"));
            Direction nf = rotationRotate(mirrorGetRotation(mir, facing), facing);
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // --- StandingSignBlock/CeilingHangingSignBlock: mirror ROTATION_16 via Mirror.mirror(int,16).
        //     StandingSignBlock.java:76-79, CeilingHangingSignBlock.java:163-166 ---
        if (f == "StandingSignBlock" || f == "CeilingHangingSignBlock") {
            int v = std::stoi(getProp(s.props, "rotation"));
            std::string np;
            if (!replaceProp(s.props, "rotation", std::to_string(mirrorMirrorInt(mir, v, 16)), np)) return -1;
            return lookup(s.name, np);
        }
        // WallBlock/CrossCollision/RedStoneWire/MossyCarpet/Vine — LR swaps N<->S, FB swaps E<->W.
        if (f == "WallBlock" || f == "CrossCollisionBlock" || f == "RedStoneWireBlock" ||
            f == "MossyCarpetBlock" || f == "VineBlock") {
            std::string np; if (!mirrorNESW(s.props, mir, np)) return -1; return lookup(s.name, np);
        }
        if (f == "MultifaceBlock") {                 // mapDirections by mirror.mirror
            std::string np; if (!mapMultifaceMirror(s.props, mir, np)) return -1; return lookup(s.name, np);
        }
        // BannerBlock/SkullBlock — ROTATION_16 via Mirror.mirror(int,16).
        if (f == "BannerBlock" || f == "SkullBlock") {
            int v = std::stoi(getProp(s.props, "rotation"));
            std::string np;
            if (!replaceProp(s.props, "rotation", std::to_string(mirrorMirrorInt(mir, v, 16)), np)) return -1;
            return lookup(s.name, np);
        }
        // FACING families: mirror = rotate(mirror.getRotation(FACING)) — i.e. FACING-only rotate by
        // the mirror-derived rotation. (FlowerBed/LeafLitter share this idiom.)
        if (f == "FlowerBedBlock" || f == "LeafLitterBlock" || f == "WallBannerBlock" ||
            f == "WallSkullBlock" || f == "BaseCoralWallFanBlock" || f == "WallTorchBlock" ||
            f == "RedstoneWallTorchBlock" || f == "AmethystClusterBlock" || f == "CopperGolemStatueBlock" ||
            f == "LadderBlock" || f == "BellBlock" || f == "GrindstoneBlock" || f == "AttachedStemBlock") {
            Direction facing = dirByName(getProp(s.props, "facing"));
            Direction nf = rotationRotate(mirrorGetRotation(mir, facing), facing);
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // RodBlock — mirror: setValue(FACING, mirror.mirror(FACING)) (Direction-mirror, NOT getRotation).
        if (f == "RodBlock") {
            Direction nf = mirrorMirror(mir, dirByName(getProp(s.props, "facing")));
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // HugeMushroomBlock — permute 6 face bools by mirror.mirror(Direction).
        if (f == "HugeMushroomBlock") {
            std::string np;
            if (!permuteFaceBools(s.props, [&](Direction d){ return mirrorMirror(mir, d); }, np)) return -1;
            return lookup(s.name, np);
        }
        // Rails: mirror RAIL_SHAPE via BaseRailBlock switch.
        if (f == "RailBlock" || f == "PoweredRailBlock" || f == "DetectorRailBlock") {
            std::string np;
            if (!replaceProp(s.props, "shape", railMirrorShape(getProp(s.props, "shape"), mir), np)) return -1;
            return lookup(s.name, np);
        }
        // Pistons + TripWireHook: mirror = rotate(mirror.getRotation(FACING)).
        if (f == "PistonBaseBlock" || f == "PistonHeadBlock" || f == "MovingPistonBlock" ||
            f == "TripWireHookBlock") {
            Direction facing = dirByName(getProp(s.props, "facing"));
            Direction nf = rotationRotate(mirrorGetRotation(mir, facing), facing);
            std::string np;
            if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
            return lookup(s.name, np);
        }
        // TripWireBlock: NSEW mirror (LR swaps N<->S, FB swaps E<->W).
        if (f == "TripWireBlock") {
            std::string np; if (!mirrorNESW(s.props, mir, np)) return -1; return lookup(s.name, np);
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
