#pragma once

// Reusable 1:1 port of net.minecraft...BlockState.rotate(Rotation)/mirror(Mirror), certified
// byte-exact over all 29873 states by block_rotate_mirror_parity (rotate 89619/0, mirror 59746/0).
// Operates on the canonical (block-name, props) representation + a (name,props)->stateId lookup,
// so it works against either block_states.json (gates) or the engine's g_blockStates.
//
// rotate(state, selfId, rotation, declaringClass, lookup) / mirror(...): the declaringClass is the
// class that DECLARES rotate/mirror (e.g. "HorizontalDirectionalBlock") — shared by many blocks.
// Returns the transformed state id, `selfId` for identity, or -1 (lookup miss) / -2 (unported class).

#include "../levelgen/structure/templatesystem/StructureTransforms.h"

#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mc::block_rotation {

using mc::Direction;
using mc::Axis;
using mc::directionAxis;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::rotationRotate;
using mc::levelgen::structure::rotationRotateInt;
using mc::levelgen::structure::mirrorGetRotation;
using mc::levelgen::structure::mirrorMirror;
using mc::levelgen::structure::mirrorMirrorInt;

// (name, props)->stateId, or -1 if absent.
using Lookup = std::function<long(const std::string& name, const std::string& props)>;

// Per-state info: short block name + canonical props string ("facing=north,half=bottom").
struct StateInfo { std::string name; std::string props; };

inline std::vector<std::string> splitc(const std::string& s, char d) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, d)) o.push_back(c);
    return o;
}
inline Direction dirByName(const std::string& n) {
    if (n == "down") return Direction::DOWN;
    if (n == "up") return Direction::UP;
    if (n == "north") return Direction::NORTH;
    if (n == "south") return Direction::SOUTH;
    if (n == "west") return Direction::WEST;
    return Direction::EAST;
}
inline const char* dirName(Direction d) {
    switch (d) {
        case Direction::DOWN: return "down";
        case Direction::UP: return "up";
        case Direction::NORTH: return "north";
        case Direction::SOUTH: return "south";
        case Direction::WEST: return "west";
        default: return "east";
    }
}
// Substitute a property value, preserving the exact key order/formatting. False if key absent.
inline bool replaceProp(const std::string& props, const std::string& key,
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
inline std::string getProp(const std::string& props, const std::string& key) {
    for (auto& p : splitc(props, ',')) {
        auto eq = p.find('=');
        if (eq != std::string::npos && p.substr(0, eq) == key) return p.substr(eq + 1);
    }
    return "";
}
// {north,east,south,west} permute (WallBlock/CrossCollision/RedStoneWire/MossyCarpet/Vine/TripWire).
inline bool permuteNESW(const std::string& props, Rotation rot, std::string& out) {
    std::string n = getProp(props, "north"), e = getProp(props, "east");
    std::string s = getProp(props, "south"), w = getProp(props, "west");
    if (n.empty() || e.empty() || s.empty() || w.empty()) return false;
    std::string nn, ne, ns, nw;
    switch (rot) {
        case Rotation::CLOCKWISE_180:       nn = s; ne = w; ns = n; nw = e; break;
        case Rotation::COUNTERCLOCKWISE_90: nn = e; ne = s; ns = w; nw = n; break;
        case Rotation::CLOCKWISE_90:        nn = w; ne = n; ns = e; nw = s; break;
        default:                            nn = n; ne = e; ns = s; nw = w; break;
    }
    std::string t;
    if (!replaceProp(props, "north", nn, t)) return false; out = t;
    if (!replaceProp(out,   "east",  ne, t)) return false; out = t;
    if (!replaceProp(out,   "south", ns, t)) return false; out = t;
    if (!replaceProp(out,   "west",  nw, t)) return false; out = t;
    return true;
}
inline bool mirrorNESW(const std::string& props, Mirror mir, std::string& out) {
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
    } else { out = props; }
    return true;
}
inline bool mapMultifaceRotate(const std::string& props, Rotation rot, std::string& out) {
    static const Direction FACES[6] = { Direction::DOWN, Direction::UP, Direction::NORTH,
                                        Direction::SOUTH, Direction::WEST, Direction::EAST };
    std::string oldVal[6];
    for (int i = 0; i < 6; ++i) { oldVal[i] = getProp(props, dirName(FACES[i])); if (oldVal[i].empty()) return false; }
    out = props; std::string t;
    for (int i = 0; i < 6; ++i) { if (!replaceProp(out, dirName(rotationRotate(rot, FACES[i])), oldVal[i], t)) return false; out = t; }
    return true;
}
inline bool mapMultifaceMirror(const std::string& props, Mirror mir, std::string& out) {
    static const Direction FACES[6] = { Direction::DOWN, Direction::UP, Direction::NORTH,
                                        Direction::SOUTH, Direction::WEST, Direction::EAST };
    std::string oldVal[6];
    for (int i = 0; i < 6; ++i) { oldVal[i] = getProp(props, dirName(FACES[i])); if (oldVal[i].empty()) return false; }
    out = props; std::string t;
    for (int i = 0; i < 6; ++i) { if (!replaceProp(out, dirName(mirrorMirror(mir, FACES[i])), oldVal[i], t)) return false; out = t; }
    return true;
}
// HugeMushroomBlock: permute the 6 directional booleans (result[transform(d)] = old[d]).
template <typename TransformDir>
bool permuteFaceBools(const std::string& props, TransformDir xf, std::string& out) {
    static const char* kDirs[6] = { "north", "south", "east", "west", "up", "down" };
    std::map<std::string, std::string> dest;
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
// BaseRailBlock RAIL_SHAPE switch (rotate :151-233 / mirror :235-281).
inline std::string railRotateShape(const std::string& shape, Rotation rot) {
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
        default: return shape;
    }
}
inline std::string railMirrorShape(const std::string& shape, Mirror mir) {
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
        default: return shape;
    }
}
// JigsawBlock/CrafterBlock ORIENTATION = FrontAndTop ("front_top"); apply xf to both front+top.
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

// ── BlockState.rotate(Rotation) — dispatch on the rotate declaring class ──
inline long rotate(const StateInfo& s, long selfId, Rotation rot, const std::string& f, const Lookup& lookup) {
    if (f == "BlockBehaviour") return selfId;
    if (f == "HorizontalDirectionalBlock" ||
        f == "ChestBlock" || f == "EnderChestBlock" || f == "BarrelBlock" ||
        f == "AbstractFurnaceBlock" || f == "DispenserBlock" || f == "ObserverBlock" ||
        f == "HopperBlock" || f == "BeehiveBlock" || f == "CampfireBlock" ||
        f == "LecternBlock" || f == "StonecutterBlock" || f == "SmallDripleafBlock" ||
        f == "DecoratedPotBlock" || f == "VaultBlock" || f == "CommandBlock" ||
        f == "ShulkerBoxBlock" || f == "CalibratedSculkSensorBlock" || f == "EndPortalFrameBlock" ||
        f == "FlowerBedBlock" || f == "LeafLitterBlock" || f == "WallBannerBlock" ||
        f == "WallSkullBlock" || f == "BaseCoralWallFanBlock" || f == "WallTorchBlock" ||
        f == "RedstoneWallTorchBlock" || f == "AmethystClusterBlock" || f == "CopperGolemStatueBlock" ||
        f == "RodBlock" || f == "LadderBlock" || f == "BellBlock" || f == "GrindstoneBlock" ||
        f == "AttachedStemBlock" || f == "AnvilBlock" ||
        f == "StairBlock" || f == "DoorBlock" || f == "WallSignBlock" ||
        f == "WallHangingSignBlock" || f == "ShelfBlock" || f == "ChiseledBookShelfBlock" ||
        f == "PistonBaseBlock" || f == "PistonHeadBlock" || f == "MovingPistonBlock" || f == "TripWireHookBlock") {
        Direction nf = rotationRotate(rot, dirByName(getProp(s.props, "facing")));
        std::string np;
        if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "JigsawBlock" || f == "CrafterBlock") {
        std::string np;
        if (!transformOrientation(s.props, [&](Direction d){ return rotationRotate(rot, d); }, np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "RotatedPillarBlock" || f == "CreakingHeartBlock" ||
        f == "InfestedRotatedPillarBlock" || f == "NetherPortalBlock") {
        if (rot == Rotation::CLOCKWISE_90 || rot == Rotation::COUNTERCLOCKWISE_90) {
            std::string ax = getProp(s.props, "axis");
            std::string na = ax == "x" ? "z" : (ax == "z" ? "x" : ax);
            std::string np;
            if (!replaceProp(s.props, "axis", na, np)) return -1;
            return lookup(s.name, np);
        }
        return selfId;
    }
    if (f == "StandingSignBlock" || f == "CeilingHangingSignBlock" || f == "BannerBlock" || f == "SkullBlock") {
        int v = std::stoi(getProp(s.props, "rotation"));
        std::string np;
        if (!replaceProp(s.props, "rotation", std::to_string(rotationRotateInt(rot, v, 16)), np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "WallBlock" || f == "CrossCollisionBlock" || f == "RedStoneWireBlock" ||
        f == "MossyCarpetBlock" || f == "VineBlock" || f == "TripWireBlock") {
        std::string np; if (!permuteNESW(s.props, rot, np)) return -1; return lookup(s.name, np);
    }
    if (f == "MultifaceBlock") {
        std::string np; if (!mapMultifaceRotate(s.props, rot, np)) return -1; return lookup(s.name, np);
    }
    if (f == "HugeMushroomBlock") {
        std::string np;
        if (!permuteFaceBools(s.props, [&](Direction d){ return rotationRotate(rot, d); }, np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "RailBlock" || f == "PoweredRailBlock" || f == "DetectorRailBlock") {
        std::string np;
        if (!replaceProp(s.props, "shape", railRotateShape(getProp(s.props, "shape"), rot), np)) return -1;
        return lookup(s.name, np);
    }
    return -2;
}

// ── BlockState.mirror(Mirror) — dispatch on the mirror declaring class ──
inline long mirror(const StateInfo& s, long selfId, Mirror mir, const std::string& f, const Lookup& lookup) {
    if (f == "BlockBehaviour") return selfId;
    if (f == "HorizontalDirectionalBlock" ||
        f == "ChestBlock" || f == "EnderChestBlock" || f == "BarrelBlock" ||
        f == "AbstractFurnaceBlock" || f == "DispenserBlock" || f == "ObserverBlock" ||
        f == "HopperBlock" || f == "BeehiveBlock" || f == "CampfireBlock" ||
        f == "LecternBlock" || f == "StonecutterBlock" || f == "SmallDripleafBlock" ||
        f == "DecoratedPotBlock" || f == "VaultBlock" || f == "CommandBlock" ||
        f == "ShulkerBoxBlock" || f == "CalibratedSculkSensorBlock" || f == "EndPortalFrameBlock" ||
        f == "FlowerBedBlock" || f == "LeafLitterBlock" || f == "WallBannerBlock" ||
        f == "WallSkullBlock" || f == "BaseCoralWallFanBlock" || f == "WallTorchBlock" ||
        f == "RedstoneWallTorchBlock" || f == "AmethystClusterBlock" || f == "CopperGolemStatueBlock" ||
        f == "LadderBlock" || f == "BellBlock" || f == "GrindstoneBlock" || f == "AttachedStemBlock" ||
        f == "WallSignBlock" || f == "WallHangingSignBlock" || f == "ShelfBlock" || f == "ChiseledBookShelfBlock" ||
        f == "PistonBaseBlock" || f == "PistonHeadBlock" || f == "MovingPistonBlock" || f == "TripWireHookBlock") {
        // mirror = state.rotate(mirror.getRotation(FACING)) -> FACING-only rotate by the derived rotation.
        Direction facing = dirByName(getProp(s.props, "facing"));
        Direction nf = rotationRotate(mirrorGetRotation(mir, facing), facing);
        std::string np;
        if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "JigsawBlock" || f == "CrafterBlock") {
        std::string np;
        if (!transformOrientation(s.props, [&](Direction d){ return mirrorMirror(mir, d); }, np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "StairBlock") {
        Axis ax = directionAxis(dirByName(getProp(s.props, "facing")));
        std::string shape = getProp(s.props, "shape");
        Direction nf = rotationRotate(Rotation::CLOCKWISE_180, dirByName(getProp(s.props, "facing")));
        std::string np;
        if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
        std::string newShape;
        if (mir == Mirror::LEFT_RIGHT && ax == Axis::Z) {
            if (shape == "outer_left") newShape = "outer_right";
            else if (shape == "inner_right") newShape = "inner_left";
            else if (shape == "inner_left") newShape = "inner_right";
            else if (shape == "outer_right") newShape = "outer_left";
        } else if (mir == Mirror::FRONT_BACK && ax == Axis::X) {
            if (shape == "outer_left") newShape = "outer_right";
            else if (shape == "outer_right") newShape = "outer_left";
        } else {
            return selfId;
        }
        if (!newShape.empty()) {
            std::string np2;
            if (!replaceProp(np, "shape", newShape, np2)) return -1;
            np = np2;
        }
        return lookup(s.name, np);
    }
    if (f == "DoorBlock") {
        if (mir == Mirror::NONE) return selfId;
        Direction facing = dirByName(getProp(s.props, "facing"));
        Direction nf = rotationRotate(mirrorGetRotation(mir, facing), facing);
        std::string np;
        if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
        std::string hinge = getProp(np, "hinge");
        std::string np2;
        if (!replaceProp(np, "hinge", hinge == "left" ? "right" : "left", np2)) return -1;
        return lookup(s.name, np2);
    }
    if (f == "StandingSignBlock" || f == "CeilingHangingSignBlock" || f == "BannerBlock" || f == "SkullBlock") {
        int v = std::stoi(getProp(s.props, "rotation"));
        std::string np;
        if (!replaceProp(s.props, "rotation", std::to_string(mirrorMirrorInt(mir, v, 16)), np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "WallBlock" || f == "CrossCollisionBlock" || f == "RedStoneWireBlock" ||
        f == "MossyCarpetBlock" || f == "VineBlock" || f == "TripWireBlock") {
        std::string np; if (!mirrorNESW(s.props, mir, np)) return -1; return lookup(s.name, np);
    }
    if (f == "MultifaceBlock") {
        std::string np; if (!mapMultifaceMirror(s.props, mir, np)) return -1; return lookup(s.name, np);
    }
    if (f == "RodBlock") {
        Direction nf = mirrorMirror(mir, dirByName(getProp(s.props, "facing")));
        std::string np;
        if (!replaceProp(s.props, "facing", dirName(nf), np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "HugeMushroomBlock") {
        std::string np;
        if (!permuteFaceBools(s.props, [&](Direction d){ return mirrorMirror(mir, d); }, np)) return -1;
        return lookup(s.name, np);
    }
    if (f == "RailBlock" || f == "PoweredRailBlock" || f == "DetectorRailBlock") {
        std::string np;
        if (!replaceProp(s.props, "shape", railMirrorShape(getProp(s.props, "shape"), mir), np)) return -1;
        return lookup(s.name, np);
    }
    return -2;
}

}  // namespace mc::block_rotation
