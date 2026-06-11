// Parity gate for net.minecraft...BlockState.getCollisionShape over every state — the per-block
// collision VoxelShapes (foundational for collision, occlusion, and updateFromNeighbourShapes'
// isFaceSturdy). Ground truth: tools/BlockCollisionShapeParity.java (SHAPE rows = the real
// collision shape as a canonical AABB list; FAM rows = the getCollisionShape/getShape declaring
// classes per block).
//
// Certified FAMILY-BY-FAMILY (declaring-class keyed, like block_rotate_mirror_parity). This run
// certifies the fully-DEFAULT family — blocks whose getCollisionShape AND getShape are both the
// BlockBehaviour default: their collision shape is the unit cube when the block has collision
// (is_solid / blocksMotion), else empty. The ~140 custom-shape families (slabs/stairs/fences/
// walls/panes/...) are an explicit fan-out worklist (printed), not silently passed (RULE #0).
//
//   block_collision_shape_parity [--cases mcpp/build/block_collision_shape.tsv]
//                                [--states mcpp/src/assets/block_states.json]

#include "../../phys/shapes/Shapes.h"
#include "../../phys/shapes/VoxelShape.h"
#include "../../phys/shapes/BooleanOp.h"
#include "../../phys/AABB.h"
#include "../../phys/Direction.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {
std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    if (!o.empty() && !o.back().empty() && o.back().back() == '\r') o.back().pop_back();
    return o;
}
std::string getProp(const std::string& props, const std::string& key) {
    std::istringstream ss(props); std::string p;
    while (std::getline(ss, p, ',')) {
        auto eq = p.find('=');
        if (eq != std::string::npos && p.substr(0, eq) == key) return p.substr(eq + 1);
    }
    return "";
}
struct Box { double x1,y1,z1,x2,y2,z2; bool operator==(const Box&) const = default; };
using Shape = std::vector<Box>;
const Shape CUBE = { {0,0,0,1,1,1} };
const Shape EMPTY = {};

// canonical (sorted) Box list from a VoxelShape (matches the GT's sorted toAabbs()).
Shape toBoxes(const mc::VoxelShapePtr& sh) {
    Shape out;
    for (const mc::AABB& a : sh->toAabbs())
        out.push_back({ a.minCorner.x, a.minCorner.y, a.minCorner.z, a.maxCorner.x, a.maxCorner.y, a.maxCorner.z });
    std::sort(out.begin(), out.end(), [](const Box& p, const Box& q){
        return std::tie(p.x1,p.y1,p.z1,p.x2,p.y2,p.z2) < std::tie(q.x1,q.y1,q.z1,q.x2,q.y2,q.z2); });
    return out;
}

// Per-family getShape (1:1 from each block class). Returns nullptr if the family is not yet ported.
// The C++ Shapes/VoxelShape primitives are certified (voxel_shapes gates).
mc::VoxelShapePtr buildFamilyShape(const std::string& fam, const std::string& name, const std::string& props) {
    using mc::Shapes;
    // SlabBlock.getShape :59-65 — TOP/BOTTOM half-boxes, DOUBLE = full cube.
    if (fam == "SlabBlock") {
        std::string type = getProp(props, "type");
        if (type == "double") return Shapes::block();
        if (type == "top") return Shapes::box(0, 0.5, 0, 1, 1, 1);
        return Shapes::box(0, 0, 0, 1, 0.5, 1);  // bottom
    }

    // ── agent C (complex shape-table families). Transcribed 1:1 from the REAL net.minecraft
    // getCollisionShape().toAabbs() (RULE #0; tools/CollisionShapeCppGen.java emitted these from
    // the 26.1.2 classes). Each branch is the Shapes::or_ fold of the canonical AABB list;
    // tools/CollisionShapeRoundTripCheck.java PROVED this or-fold re-discretizes through toAabbs()
    // to the byte-identical vanilla box set on ALL states (0 failures: Stair 4640, Door 1344,
    // Bed 256, Chest 240, Campfire 64, Bell 32, Grindstone/Anvil 12, Lectern 16, BrewingStand 8,
    // Stonecutter 4, EnchantingTable 1). ──

    // StairBlock.getShape :74-89 — HALF (top/bottom) x SHAPE (straight/inner/outer) x FACING
    // composing SHAPE_BOTTOM/TOP_{OUTER,STRAIGHT,INNER} (:37-45). 40 distinct shapes.
    if (fam == "StairBlock") {
        std::string facing = getProp(props, "facing");
        std::string half = getProp(props, "half");
        std::string shape = getProp(props, "shape");
        if (facing == "north" && half == "top" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 1, 0.5), Shapes::box(0, 0.5, 0.5, 1, 1, 1));
        else if (facing == "north" && half == "top" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "north" && half == "top" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 1, 0.5), Shapes::box(0, 0.5, 0.5, 0.5, 1, 1)), Shapes::box(0.5, 0, 0.5, 1, 1, 1));
        else if (facing == "north" && half == "top" && shape == "outer_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 0.5), Shapes::box(0, 0.5, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "north" && half == "top" && shape == "outer_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.5, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "north" && half == "bottom" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 1, 1, 0.5));
        else if (facing == "north" && half == "bottom" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "north" && half == "bottom" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "north" && half == "bottom" && shape == "outer_left")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 0.5));
        else if (facing == "north" && half == "bottom" && shape == "outer_right")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "south" && half == "top" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0.5, 1, 1, 1), Shapes::box(0, 0.5, 0, 1, 1, 0.5));
        else if (facing == "south" && half == "top" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0.5, 1, 1, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 0.5)), Shapes::box(0.5, 0, 0, 1, 1, 0.5));
        else if (facing == "south" && half == "top" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "south" && half == "top" && shape == "outer_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.5, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "south" && half == "top" && shape == "outer_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0.5, 0.5, 1, 1), Shapes::box(0, 0.5, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "south" && half == "bottom" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0.5, 1, 1, 1));
        else if (facing == "south" && half == "bottom" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "south" && half == "bottom" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 1)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "south" && half == "bottom" && shape == "outer_left")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "south" && half == "bottom" && shape == "outer_right")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0.5, 0.5, 1, 1));
        else if (facing == "west" && half == "top" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 1), Shapes::box(0.5, 0.5, 0, 1, 1, 1));
        else if (facing == "west" && half == "top" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "west" && half == "top" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "west" && half == "top" && shape == "outer_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0.5, 0.5, 1, 1), Shapes::box(0, 0.5, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "west" && half == "top" && shape == "outer_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.5, 1, 0.5), Shapes::box(0, 0.5, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "west" && half == "bottom" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 1));
        else if (facing == "west" && half == "bottom" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 1)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "west" && half == "bottom" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "west" && half == "bottom" && shape == "outer_left")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0.5, 0.5, 1, 1));
        else if (facing == "west" && half == "bottom" && shape == "outer_right")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 0.5));
        else if (facing == "east" && half == "top" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0.5, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0, 1, 1, 1));
        else if (facing == "east" && half == "top" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 1, 0.5), Shapes::box(0, 0.5, 0.5, 0.5, 1, 1)), Shapes::box(0.5, 0, 0.5, 1, 1, 1));
        else if (facing == "east" && half == "top" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0.5, 1, 1, 1), Shapes::box(0, 0.5, 0, 0.5, 1, 0.5)), Shapes::box(0.5, 0, 0, 1, 1, 0.5));
        else if (facing == "east" && half == "top" && shape == "outer_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.5, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "east" && half == "top" && shape == "outer_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.5, 0, 0.5, 1, 1), Shapes::box(0.5, 0, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "east" && half == "bottom" && shape == "straight")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0.5, 0.5, 0, 1, 1, 1));
        else if (facing == "east" && half == "bottom" && shape == "inner_left")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0, 1, 1, 0.5)), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        else if (facing == "east" && half == "bottom" && shape == "inner_right")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0, 0.5, 0.5, 1, 1, 1)), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "east" && half == "bottom" && shape == "outer_left")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0.5, 0.5, 0, 1, 1, 0.5));
        else if (facing == "east" && half == "bottom" && shape == "outer_right")
            return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.5, 1), Shapes::box(0.5, 0.5, 0.5, 1, 1, 1));
        return nullptr;  // unreached: all states enumerated
    }

    // DoorBlock.getShape :78-85 — thin 3/16 box on the hinge edge; doorDirection from
    // FACING + OPEN + HINGE (SHAPES = rotateHorizontal(boxZ(16,13,16)), :52). 32 shapes.
    if (fam == "DoorBlock") {
        std::string facing = getProp(props, "facing");
        std::string half = getProp(props, "half");
        std::string hinge = getProp(props, "hinge");
        std::string open = getProp(props, "open");
        if (facing == "north" && half == "upper" && hinge == "left" && open == "true")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "north" && half == "upper" && hinge == "left" && open == "false")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "north" && half == "upper" && hinge == "right" && open == "true")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "north" && half == "upper" && hinge == "right" && open == "false")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "north" && half == "lower" && hinge == "left" && open == "true")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "north" && half == "lower" && hinge == "left" && open == "false")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "north" && half == "lower" && hinge == "right" && open == "true")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "north" && half == "lower" && hinge == "right" && open == "false")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "south" && half == "upper" && hinge == "left" && open == "true")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "south" && half == "upper" && hinge == "left" && open == "false")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "south" && half == "upper" && hinge == "right" && open == "true")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "south" && half == "upper" && hinge == "right" && open == "false")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "south" && half == "lower" && hinge == "left" && open == "true")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "south" && half == "lower" && hinge == "left" && open == "false")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "south" && half == "lower" && hinge == "right" && open == "true")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "south" && half == "lower" && hinge == "right" && open == "false")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "west" && half == "upper" && hinge == "left" && open == "true")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "west" && half == "upper" && hinge == "left" && open == "false")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "west" && half == "upper" && hinge == "right" && open == "true")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "west" && half == "upper" && hinge == "right" && open == "false")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "west" && half == "lower" && hinge == "left" && open == "true")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "west" && half == "lower" && hinge == "left" && open == "false")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "west" && half == "lower" && hinge == "right" && open == "true")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "west" && half == "lower" && hinge == "right" && open == "false")
            return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        else if (facing == "east" && half == "upper" && hinge == "left" && open == "true")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "east" && half == "upper" && hinge == "left" && open == "false")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "east" && half == "upper" && hinge == "right" && open == "true")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "east" && half == "upper" && hinge == "right" && open == "false")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "east" && half == "lower" && hinge == "left" && open == "true")
            return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        else if (facing == "east" && half == "lower" && hinge == "left" && open == "false")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        else if (facing == "east" && half == "lower" && hinge == "right" && open == "true")
            return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        else if (facing == "east" && half == "lower" && hinge == "right" && open == "false")
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);
        return nullptr;  // unreached: all states enumerated
    }

    // BedBlock.getShape :207-209 — base column(16,3,9) + two 3x3x3 legs, rotateHorizontal
    // keyed by getConnectedDirection(state).getOpposite() (:57-61). PART x FACING. 8 shapes.
    if (fam == "BedBlock") {
        std::string facing = getProp(props, "facing");
        std::string part = getProp(props, "part");
        if (facing == "north" && part == "head")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.1875, 0.5625, 0.1875), Shapes::box(0, 0.1875, 0.1875, 1, 0.5625, 1)), Shapes::box(0.1875, 0.1875, 0, 0.8125, 0.5625, 0.1875)), Shapes::box(0.8125, 0, 0, 1, 0.5625, 0.1875));
        else if (facing == "north" && part == "foot")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0.8125, 0.1875, 0.5625, 1), Shapes::box(0, 0.1875, 0, 1, 0.5625, 0.8125)), Shapes::box(0.1875, 0.1875, 0.8125, 0.8125, 0.5625, 1)), Shapes::box(0.8125, 0, 0.8125, 1, 0.5625, 1));
        else if (facing == "south" && part == "head")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0.8125, 0.1875, 0.5625, 1), Shapes::box(0, 0.1875, 0, 1, 0.5625, 0.8125)), Shapes::box(0.1875, 0.1875, 0.8125, 0.8125, 0.5625, 1)), Shapes::box(0.8125, 0, 0.8125, 1, 0.5625, 1));
        else if (facing == "south" && part == "foot")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.1875, 0.5625, 0.1875), Shapes::box(0, 0.1875, 0.1875, 1, 0.5625, 1)), Shapes::box(0.1875, 0.1875, 0, 0.8125, 0.5625, 0.1875)), Shapes::box(0.8125, 0, 0, 1, 0.5625, 0.1875));
        else if (facing == "west" && part == "head")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.1875, 0.5625, 0.1875), Shapes::box(0, 0, 0.8125, 0.1875, 0.5625, 1)), Shapes::box(0, 0.1875, 0.1875, 1, 0.5625, 0.8125)), Shapes::box(0.1875, 0.1875, 0, 1, 0.5625, 0.1875)), Shapes::box(0.1875, 0.1875, 0.8125, 1, 0.5625, 1));
        else if (facing == "west" && part == "foot")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0.1875, 0, 0.8125, 0.5625, 1), Shapes::box(0.8125, 0, 0, 1, 0.5625, 0.1875)), Shapes::box(0.8125, 0, 0.8125, 1, 0.5625, 1)), Shapes::box(0.8125, 0.1875, 0.1875, 1, 0.5625, 0.8125));
        else if (facing == "east" && part == "head")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0.1875, 0, 0.8125, 0.5625, 1), Shapes::box(0.8125, 0, 0, 1, 0.5625, 0.1875)), Shapes::box(0.8125, 0, 0.8125, 1, 0.5625, 1)), Shapes::box(0.8125, 0.1875, 0.1875, 1, 0.5625, 0.8125));
        else if (facing == "east" && part == "foot")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0, 0, 0.1875, 0.5625, 0.1875), Shapes::box(0, 0, 0.8125, 0.1875, 0.5625, 1)), Shapes::box(0, 0.1875, 0.1875, 1, 0.5625, 0.8125)), Shapes::box(0.1875, 0.1875, 0, 1, 0.5625, 0.1875)), Shapes::box(0.1875, 0.1875, 0.8125, 1, 0.5625, 1));
        return nullptr;  // unreached: all states enumerated
    }

    // BellBlock.getCollisionShape :167-168 -> getVoxelShape :155-163 — ATTACHMENT (floor/
    // ceiling/single_wall/double_wall) x FACING; BELL_SHAPE + post/yoke per case (:49-55). 16.
    if (fam == "BellBlock") {
        std::string attachment = getProp(props, "attachment");
        std::string facing = getProp(props, "facing");
        if (attachment == "floor" && facing == "north")
            return Shapes::box(0, 0, 0.25, 1, 1, 0.75);
        else if (attachment == "floor" && facing == "south")
            return Shapes::box(0, 0, 0.25, 1, 1, 0.75);
        else if (attachment == "floor" && facing == "west")
            return Shapes::box(0.25, 0, 0, 0.75, 1, 1);
        else if (attachment == "floor" && facing == "east")
            return Shapes::box(0.25, 0, 0, 0.75, 1, 1);
        else if (attachment == "ceiling" && facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0.4375, 0.5625, 1, 0.5625));
        else if (attachment == "ceiling" && facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0.4375, 0.5625, 1, 0.5625));
        else if (attachment == "ceiling" && facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0.4375, 0.5625, 1, 0.5625));
        else if (attachment == "ceiling" && facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0.4375, 0.5625, 1, 0.5625));
        else if (attachment == "single_wall" && facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0, 0.5625, 0.9375, 0.8125));
        else if (attachment == "single_wall" && facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0.1875, 0.5625, 0.9375, 1));
        else if (attachment == "single_wall" && facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.8125, 0.4375, 0.8125, 0.9375, 0.5625), Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75)), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875));
        else if (attachment == "single_wall" && facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::box(0.1875, 0.8125, 0.4375, 1, 0.9375, 0.5625), Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75)), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875));
        else if (attachment == "double_wall" && facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0, 0.5625, 0.9375, 1));
        else if (attachment == "double_wall" && facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875)), Shapes::box(0.4375, 0.8125, 0, 0.5625, 0.9375, 1));
        else if (attachment == "double_wall" && facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.8125, 0.4375, 1, 0.9375, 0.5625), Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75)), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875));
        else if (attachment == "double_wall" && facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::box(0, 0.8125, 0.4375, 1, 0.9375, 0.5625), Shapes::box(0.25, 0.25, 0.25, 0.75, 0.375, 0.75)), Shapes::box(0.3125, 0.375, 0.3125, 0.6875, 0.8125, 0.6875));
        return nullptr;  // unreached: all states enumerated
    }

    // GrindstoneBlock.getCollisionShape :59-60 -> makeShapes :46-51 — pivot + two legs,
    // rotateAttachFace keyed by FACE (floor/wall/ceiling) x FACING. 12 shapes.
    if (fam == "GrindstoneBlock") {
        std::string face = getProp(props, "face");
        std::string facing = getProp(props, "facing");
        if (face == "floor" && facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0, 0.375, 0.25, 0.8125, 0.625), Shapes::box(0.125, 0.4375, 0.3125, 0.25, 0.8125, 0.375)), Shapes::box(0.125, 0.4375, 0.625, 0.25, 0.8125, 0.6875)), Shapes::box(0.25, 0.25, 0.125, 0.75, 1, 0.875)), Shapes::box(0.75, 0, 0.375, 0.875, 0.8125, 0.625)), Shapes::box(0.75, 0.4375, 0.3125, 0.875, 0.8125, 0.375)), Shapes::box(0.75, 0.4375, 0.625, 0.875, 0.8125, 0.6875));
        else if (face == "floor" && facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0, 0.375, 0.25, 0.8125, 0.625), Shapes::box(0.125, 0.4375, 0.3125, 0.25, 0.8125, 0.375)), Shapes::box(0.125, 0.4375, 0.625, 0.25, 0.8125, 0.6875)), Shapes::box(0.25, 0.25, 0.125, 0.75, 1, 0.875)), Shapes::box(0.75, 0, 0.375, 0.875, 0.8125, 0.625)), Shapes::box(0.75, 0.4375, 0.3125, 0.875, 0.8125, 0.375)), Shapes::box(0.75, 0.4375, 0.625, 0.875, 0.8125, 0.6875));
        else if (face == "floor" && facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0.25, 0.25, 0.875, 1, 0.75), Shapes::box(0.3125, 0.4375, 0.125, 0.375, 0.8125, 0.25)), Shapes::box(0.3125, 0.4375, 0.75, 0.375, 0.8125, 0.875)), Shapes::box(0.375, 0, 0.125, 0.625, 0.8125, 0.25)), Shapes::box(0.375, 0, 0.75, 0.625, 0.8125, 0.875)), Shapes::box(0.625, 0.4375, 0.125, 0.6875, 0.8125, 0.25)), Shapes::box(0.625, 0.4375, 0.75, 0.6875, 0.8125, 0.875));
        else if (face == "floor" && facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0.25, 0.25, 0.875, 1, 0.75), Shapes::box(0.3125, 0.4375, 0.125, 0.375, 0.8125, 0.25)), Shapes::box(0.3125, 0.4375, 0.75, 0.375, 0.8125, 0.875)), Shapes::box(0.375, 0, 0.125, 0.625, 0.8125, 0.25)), Shapes::box(0.375, 0, 0.75, 0.625, 0.8125, 0.875)), Shapes::box(0.625, 0.4375, 0.125, 0.6875, 0.8125, 0.25)), Shapes::box(0.625, 0.4375, 0.75, 0.6875, 0.8125, 0.875));
        else if (face == "wall" && facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0.3125, 0.1875, 0.25, 0.6875, 0.5625), Shapes::box(0.125, 0.375, 0.5625, 0.25, 0.625, 1)), Shapes::box(0.25, 0.125, 0, 0.75, 0.875, 0.75)), Shapes::box(0.75, 0.3125, 0.1875, 0.875, 0.6875, 0.5625)), Shapes::box(0.75, 0.375, 0.5625, 0.875, 0.625, 1));
        else if (face == "wall" && facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0.3125, 0.4375, 0.25, 0.6875, 0.8125), Shapes::box(0.125, 0.375, 0, 0.25, 0.625, 0.4375)), Shapes::box(0.25, 0.125, 0.25, 0.75, 0.875, 1)), Shapes::box(0.75, 0.3125, 0.4375, 0.875, 0.6875, 0.8125)), Shapes::box(0.75, 0.375, 0, 0.875, 0.625, 0.4375));
        else if (face == "wall" && facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0.125, 0.25, 0.75, 0.875, 0.75), Shapes::box(0.1875, 0.3125, 0.125, 0.5625, 0.6875, 0.25)), Shapes::box(0.1875, 0.3125, 0.75, 0.5625, 0.6875, 0.875)), Shapes::box(0.5625, 0.375, 0.125, 1, 0.625, 0.25)), Shapes::box(0.5625, 0.375, 0.75, 1, 0.625, 0.875));
        else if (face == "wall" && facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0.375, 0.125, 0.4375, 0.625, 0.25), Shapes::box(0, 0.375, 0.75, 0.4375, 0.625, 0.875)), Shapes::box(0.25, 0.125, 0.25, 1, 0.875, 0.75)), Shapes::box(0.4375, 0.3125, 0.125, 0.8125, 0.6875, 0.25)), Shapes::box(0.4375, 0.3125, 0.75, 0.8125, 0.6875, 0.875));
        else if (face == "ceiling" && facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0.1875, 0.3125, 0.25, 0.5625, 0.6875), Shapes::box(0.125, 0.5625, 0.375, 0.25, 1, 0.625)), Shapes::box(0.25, 0, 0.125, 0.75, 0.75, 0.875)), Shapes::box(0.75, 0.1875, 0.3125, 0.875, 0.5625, 0.6875)), Shapes::box(0.75, 0.5625, 0.375, 0.875, 1, 0.625));
        else if (face == "ceiling" && facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0.1875, 0.3125, 0.25, 0.5625, 0.6875), Shapes::box(0.125, 0.5625, 0.375, 0.25, 1, 0.625)), Shapes::box(0.25, 0, 0.125, 0.75, 0.75, 0.875)), Shapes::box(0.75, 0.1875, 0.3125, 0.875, 0.5625, 0.6875)), Shapes::box(0.75, 0.5625, 0.375, 0.875, 1, 0.625));
        else if (face == "ceiling" && facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0, 0.25, 0.875, 0.75, 0.75), Shapes::box(0.3125, 0.1875, 0.125, 0.6875, 0.5625, 0.25)), Shapes::box(0.3125, 0.1875, 0.75, 0.6875, 0.5625, 0.875)), Shapes::box(0.375, 0.5625, 0.125, 0.625, 1, 0.25)), Shapes::box(0.375, 0.5625, 0.75, 0.625, 1, 0.875));
        else if (face == "ceiling" && facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0, 0.25, 0.875, 0.75, 0.75), Shapes::box(0.3125, 0.1875, 0.125, 0.6875, 0.5625, 0.25)), Shapes::box(0.3125, 0.1875, 0.75, 0.6875, 0.5625, 0.875)), Shapes::box(0.375, 0.5625, 0.125, 0.625, 1, 0.25)), Shapes::box(0.375, 0.5625, 0.75, 0.625, 1, 0.875));
        return nullptr;  // unreached: all states enumerated
    }

    // AnvilBlock.getShape :77-78 — base+waist+top columns, rotateHorizontalAxis keyed by
    // FACING.getAxis() (:35-37). 4 shapes (X/Z axes each appear twice). 7 boxes each.
    if (fam == "AnvilBlock") {
        std::string facing = getProp(props, "facing");
        if (facing == "north")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0, 0.125, 0.875, 0.25, 0.875), Shapes::box(0.1875, 0.625, 0, 0.375, 1, 1)), Shapes::box(0.25, 0.25, 0.1875, 0.75, 0.3125, 0.8125)), Shapes::box(0.375, 0.3125, 0.25, 0.625, 1, 0.75)), Shapes::box(0.375, 0.625, 0, 0.8125, 1, 0.25)), Shapes::box(0.375, 0.625, 0.75, 0.8125, 1, 1)), Shapes::box(0.625, 0.625, 0.25, 0.8125, 1, 0.75));
        else if (facing == "south")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0.125, 0, 0.125, 0.875, 0.25, 0.875), Shapes::box(0.1875, 0.625, 0, 0.375, 1, 1)), Shapes::box(0.25, 0.25, 0.1875, 0.75, 0.3125, 0.8125)), Shapes::box(0.375, 0.3125, 0.25, 0.625, 1, 0.75)), Shapes::box(0.375, 0.625, 0, 0.8125, 1, 0.25)), Shapes::box(0.375, 0.625, 0.75, 0.8125, 1, 1)), Shapes::box(0.625, 0.625, 0.25, 0.8125, 1, 0.75));
        else if (facing == "west")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0.625, 0.1875, 0.25, 1, 0.8125), Shapes::box(0.125, 0, 0.125, 0.875, 0.25, 0.875)), Shapes::box(0.1875, 0.25, 0.25, 0.8125, 0.3125, 0.75)), Shapes::box(0.25, 0.3125, 0.375, 0.75, 1, 0.625)), Shapes::box(0.25, 0.625, 0.1875, 1, 1, 0.375)), Shapes::box(0.25, 0.625, 0.625, 1, 1, 0.8125)), Shapes::box(0.75, 0.625, 0.375, 1, 1, 0.625));
        else if (facing == "east")
            return Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::or_(Shapes::box(0, 0.625, 0.1875, 0.25, 1, 0.8125), Shapes::box(0.125, 0, 0.125, 0.875, 0.25, 0.875)), Shapes::box(0.1875, 0.25, 0.25, 0.8125, 0.3125, 0.75)), Shapes::box(0.25, 0.3125, 0.375, 0.75, 1, 0.625)), Shapes::box(0.25, 0.625, 0.1875, 1, 1, 0.375)), Shapes::box(0.25, 0.625, 0.625, 1, 1, 0.8125)), Shapes::box(0.75, 0.625, 0.375, 1, 1, 0.625));
        return nullptr;  // unreached: all states enumerated
    }

    // ChestBlock.getShape :191-197 — SINGLE=column(14,0,14); LEFT/RIGHT=HALF_SHAPES
    // (boxZ(14,0,14,0,15)) keyed by getConnectedDirection(state) (:199-201). 12 shapes.
    if (fam == "ChestBlock") {
        std::string facing = getProp(props, "facing");
        std::string type = getProp(props, "type");
        if (facing == "north" && type == "single")
            return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 0.9375);
        else if (facing == "north" && type == "left")
            return Shapes::box(0.0625, 0, 0.0625, 1, 0.875, 0.9375);
        else if (facing == "north" && type == "right")
            return Shapes::box(0, 0, 0.0625, 0.9375, 0.875, 0.9375);
        else if (facing == "south" && type == "single")
            return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 0.9375);
        else if (facing == "south" && type == "left")
            return Shapes::box(0, 0, 0.0625, 0.9375, 0.875, 0.9375);
        else if (facing == "south" && type == "right")
            return Shapes::box(0.0625, 0, 0.0625, 1, 0.875, 0.9375);
        else if (facing == "west" && type == "single")
            return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 0.9375);
        else if (facing == "west" && type == "left")
            return Shapes::box(0.0625, 0, 0, 0.9375, 0.875, 0.9375);
        else if (facing == "west" && type == "right")
            return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 1);
        else if (facing == "east" && type == "single")
            return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 0.9375);
        else if (facing == "east" && type == "left")
            return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 1);
        else if (facing == "east" && type == "right")
            return Shapes::box(0.0625, 0, 0, 0.9375, 0.875, 0.9375);
        return nullptr;  // unreached: all states enumerated
    }

    // LecternBlock.getCollisionShape :96-99 returns SHAPE_COLLISION (facing-independent) =
    // or(column(16,0,2), column(8,2,14)) (:49). (getShape is facing-dependent but the gate
    // compares getCollisionShape.)
    if (fam == "LecternBlock")
        return Shapes::or_(Shapes::box(0, 0, 0, 1, 0.125, 1), Shapes::box(0.25, 0.125, 0.25, 0.75, 0.875, 0.75));
    // StonecutterBlock.getShape :68-69 = SHAPE = column(16,0,9) (:31).
    if (fam == "StonecutterBlock")
        return Shapes::box(0, 0, 0, 1, 0.5625, 1);
    // EnchantingTableBlock.getShape :58-59 = SHAPE = column(16,0,12) (:36).
    if (fam == "EnchantingTableBlock")
        return Shapes::box(0, 0, 0, 1, 0.75, 1);
    // BrewingStandBlock.getShape :60-61 = SHAPE = or(column(2,2,14), column(14,0,2)) (:37).
    if (fam == "BrewingStandBlock")
        return Shapes::or_(Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.125, 0.9375), Shapes::box(0.4375, 0.125, 0.4375, 0.5625, 0.875, 0.5625));
    // CampfireBlock.getShape :162-163 = SHAPE = column(16,0,7) (:69) (no getCollisionShape
    // override -> collision == this).
    if (fam == "CampfireBlock")
        return Shapes::box(0, 0, 0, 1, 0.4375, 1);

    // ── agent A (simple-box families). GT = getCollisionShape, so hasCollision=false families -> empty. ──
    if (fam == "ButtonBlock") return Shapes::empty();  // hasCollision=false -> empty
    if (fam == "BasePressurePlateBlock" || fam == "PressurePlateBlock" || fam == "WeightedPressurePlateBlock")
        return Shapes::empty();  // hasCollision=false -> empty
    if (fam == "TrapDoorBlock") {  // SHAPES rotateAll(boxZ(16,13,16)); key = open?facing:(half==top?DOWN:UP)
        if (getProp(props, "open") == "true") {
            std::string f = getProp(props, "facing");
            if (f == "north") return Shapes::box(0, 0, 0.8125, 1, 1, 1);
            if (f == "south") return Shapes::box(0, 0, 0, 1, 1, 0.1875);
            if (f == "west")  return Shapes::box(0.8125, 0, 0, 1, 1, 1);
            return Shapes::box(0, 0, 0, 0.1875, 1, 1);  // east
        }
        if (getProp(props, "half") == "top") return Shapes::box(0, 0.8125, 0, 1, 1, 1);
        return Shapes::box(0, 0, 0, 1, 0.1875, 1);
    }
    if (fam == "CarpetBlock") return Shapes::box(0, 0, 0, 1, 0.0625, 1);
    if (fam == "SnowLayerBlock") {  // getCollisionShape = SHAPES[layers-1] = column(16,0,(layers-1)*2)
        int layers = std::stoi(getProp(props, "layers"));
        double h = (layers - 1) * 2 / 16.0;
        if (h <= 0.0) return Shapes::empty();
        return Shapes::box(0, 0, 0, 1, h, 1);
    }
    if (fam == "CakeBlock") {
        int bites = std::stoi(getProp(props, "bites"));
        return Shapes::box((1 + bites * 2) / 16.0, 0, 0.0625, 0.9375, 0.5, 0.9375);
    }
    if (fam == "LanternBlock") {
        if (getProp(props, "hanging") == "true")
            return Shapes::or_(Shapes::box(0.3125, 0.0625, 0.3125, 0.6875, 0.5, 0.6875),
                               Shapes::box(0.375, 0.5, 0.375, 0.625, 0.625, 0.625));
        return Shapes::or_(Shapes::box(0.3125, 0.0, 0.3125, 0.6875, 0.4375, 0.6875),
                           Shapes::box(0.375, 0.4375, 0.375, 0.625, 0.5625, 0.625));
    }
    if (fam == "ChainBlock") {
        std::string axis = getProp(props, "axis");
        if (axis == "x") return Shapes::box(0, 0.40625, 0.40625, 1, 0.59375, 0.59375);
        if (axis == "z") return Shapes::box(0.40625, 0.40625, 0, 0.59375, 0.59375, 1);
        return Shapes::box(0.40625, 0, 0.40625, 0.59375, 1, 0.59375);  // y
    }
    if (fam == "LadderBlock") {
        std::string f = getProp(props, "facing");
        if (f == "north") return Shapes::box(0, 0, 0.8125, 1, 1, 1);
        if (f == "south") return Shapes::box(0, 0, 0, 1, 1, 0.1875);
        if (f == "west")  return Shapes::box(0.8125, 0, 0, 1, 1, 1);
        return Shapes::box(0, 0, 0, 0.1875, 1, 1);  // east
    }
    if (fam == "RodBlock") {  // EndRod + LightningRod; rotateAllAxis(cube(4,4,16)) by FACING axis
        std::string f = getProp(props, "facing");
        if (f == "north" || f == "south") return Shapes::box(0.375, 0.375, 0, 0.625, 0.625, 1);
        if (f == "east"  || f == "west")  return Shapes::box(0, 0.375, 0.375, 1, 0.625, 0.625);
        return Shapes::box(0.375, 0, 0.375, 0.625, 1, 0.625);  // up/down -> Y
    }
    if (fam == "ScaffoldingBlock") {  // getCollisionShape(empty ctx) = SHAPE_STABLE
        mc::VoxelShapePtr s = Shapes::box(0, 0.875, 0, 1, 1, 1);
        s = Shapes::or_(s, Shapes::box(0, 0, 0, 0.125, 1, 0.125));
        s = Shapes::or_(s, Shapes::box(0, 0, 0.875, 0.125, 1, 1));
        s = Shapes::or_(s, Shapes::box(0.875, 0, 0, 1, 1, 0.125));
        s = Shapes::or_(s, Shapes::box(0.875, 0, 0.875, 1, 1, 1));
        return s;
    }
    if (fam == "ComposterBlock")  // getCollisionShape = SHAPES[0] = block minus column(12,2,16)
        return Shapes::join(Shapes::block(), Shapes::box(0.125, 0.125, 0.125, 0.875, 1, 0.875),
                            mc::BooleanOps::ONLY_FIRST);
    if (fam == "AbstractCauldronBlock") {
        mc::VoxelShapePtr cut = Shapes::or_(Shapes::box(0, 0, 0.25, 1, 0.1875, 0.75),
                                            Shapes::box(0.25, 0, 0, 0.75, 0.1875, 1));
        cut = Shapes::or_(cut, Shapes::box(0.125, 0, 0.125, 0.875, 0.1875, 0.875));
        cut = Shapes::or_(cut, Shapes::box(0.125, 0.25, 0.125, 0.875, 1, 0.875));
        return Shapes::join(Shapes::block(), cut, mc::BooleanOps::ONLY_FIRST);
    }
    if (fam == "FlowerPotBlock") return Shapes::box(0.3125, 0, 0.3125, 0.6875, 0.375, 0.6875);
    if (fam == "EnderChestBlock") return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.875, 0.9375);
    if (fam == "ConduitBlock") return Shapes::box(0.3125, 0.3125, 0.3125, 0.6875, 0.6875, 0.6875);
    if (fam == "SeaPickleBlock") {
        int p = std::stoi(getProp(props, "pickles"));
        if (p == 2) return Shapes::box(0.1875, 0, 0.1875, 0.8125, 0.375, 0.8125);
        if (p == 3) return Shapes::box(0.125, 0, 0.125, 0.875, 0.375, 0.875);
        if (p == 4) return Shapes::box(0.125, 0, 0.125, 0.875, 0.4375, 0.875);
        return Shapes::box(0.375, 0, 0.375, 0.625, 0.375, 0.625);  // 1
    }
    if (fam == "TurtleEggBlock") {
        if (std::stoi(getProp(props, "eggs")) == 1) return Shapes::box(0.1875, 0, 0.1875, 0.75, 0.4375, 0.75);
        return Shapes::box(0.0625, 0, 0.0625, 0.9375, 0.4375, 0.9375);
    }
    if (fam == "DecoratedPotBlock") return Shapes::box(0.0625, 0, 0.0625, 0.9375, 1, 0.9375);
    if (fam == "HopperBlock") {
        auto base = []() {
            mc::VoxelShapePtr s = Shapes::box(0, 0.625, 0, 0.25, 0.6875, 1);
            s = Shapes::or_(s, Shapes::box(0, 0.6875, 0, 0.125, 1, 1));
            s = Shapes::or_(s, Shapes::box(0.125, 0.6875, 0, 1, 1, 0.125));
            s = Shapes::or_(s, Shapes::box(0.125, 0.6875, 0.875, 1, 1, 1));
            s = Shapes::or_(s, Shapes::box(0.25, 0.625, 0, 1, 0.6875, 0.25));
            s = Shapes::or_(s, Shapes::box(0.25, 0.625, 0.75, 1, 0.6875, 1));
            s = Shapes::or_(s, Shapes::box(0.75, 0.625, 0.25, 1, 0.6875, 0.75));
            s = Shapes::or_(s, Shapes::box(0.875, 0.6875, 0.125, 1, 1, 0.875));
            return s;
        };
        std::string f = getProp(props, "facing");
        if (f == "down") {
            mc::VoxelShapePtr s = base();
            s = Shapes::or_(s, Shapes::box(0.25, 0.25, 0.25, 0.375, 0.6875, 0.75));
            s = Shapes::or_(s, Shapes::box(0.375, 0, 0.375, 0.625, 0.6875, 0.625));
            s = Shapes::or_(s, Shapes::box(0.375, 0.25, 0.25, 0.75, 0.6875, 0.375));
            s = Shapes::or_(s, Shapes::box(0.375, 0.25, 0.625, 0.75, 0.6875, 0.75));
            s = Shapes::or_(s, Shapes::box(0.625, 0.25, 0.375, 0.75, 0.6875, 0.625));
            return s;
        }
        mc::VoxelShapePtr s = base();
        s = Shapes::or_(s, Shapes::box(0.25, 0.25, 0.25, 0.75, 0.6875, 0.75));
        if (f == "north") s = Shapes::or_(s, Shapes::box(0.375, 0.25, 0, 0.625, 0.5, 0.25));
        else if (f == "south") s = Shapes::or_(s, Shapes::box(0.375, 0.25, 0.75, 0.625, 0.5, 1));
        else if (f == "west")  s = Shapes::or_(s, Shapes::box(0, 0.25, 0.375, 0.75, 0.5, 0.625));
        else                   s = Shapes::or_(s, Shapes::box(0.75, 0.25, 0.375, 1, 0.5, 0.625));  // east
        return s;
    }
    // ── agent C (connection blocks: post + per-direction arms). ──
    // CrossCollisionBlock.getCollisionShape — fences (w=4,H=24) vs bars/panes (w=2,H=16); name disambiguates.
    if (fam == "CrossCollisionBlock") {
        bool isFence = name.size() >= 5 && name.compare(name.size() - 5, 5, "fence") == 0;
        double w = isFence ? 4.0 : 2.0, H = isFence ? 24.0 : 16.0;
        double a = (8.0 - w / 2.0) / 16.0, b = (8.0 + w / 2.0) / 16.0, h = H / 16.0;
        mc::VoxelShapePtr shape = Shapes::box(a, 0, a, b, h, b);  // post = column(w,0,H)
        if (getProp(props, "north") == "true") shape = Shapes::or_(shape, Shapes::box(a, 0, 0.0, b, h, 0.5));
        if (getProp(props, "south") == "true") shape = Shapes::or_(shape, Shapes::box(a, 0, 0.5, b, h, 1.0));
        if (getProp(props, "west")  == "true") shape = Shapes::or_(shape, Shapes::box(0.0, 0, a, 0.5, h, b));
        if (getProp(props, "east")  == "true") shape = Shapes::or_(shape, Shapes::box(0.5, 0, a, 1.0, h, b));
        return shape;
    }
    if (fam == "WallBlock") {  // makeShapes(24,24): post (if up) + arms; collision LOW==TALL (both 24)
        mc::VoxelShapePtr shape = getProp(props, "up") == "true"
            ? Shapes::box(0.25, 0, 0.25, 0.75, 1.5, 0.75) : Shapes::empty();
        std::string n = getProp(props, "north"), e = getProp(props, "east"),
                    s = getProp(props, "south"), w = getProp(props, "west");
        if (n == "low" || n == "tall") shape = Shapes::or_(shape, Shapes::box(0.3125, 0, 0.0, 0.6875, 1.5, 0.6875));
        if (s == "low" || s == "tall") shape = Shapes::or_(shape, Shapes::box(0.3125, 0, 0.3125, 0.6875, 1.5, 1.0));
        if (w == "low" || w == "tall") shape = Shapes::or_(shape, Shapes::box(0.0, 0, 0.3125, 0.6875, 1.5, 0.6875));
        if (e == "low" || e == "tall") shape = Shapes::or_(shape, Shapes::box(0.3125, 0, 0.3125, 1.0, 1.5, 0.6875));
        return shape;
    }
    if (fam == "FenceGateBlock") {  // open -> empty; else SHAPE_COLLISION[facing.axis]
        if (getProp(props, "open") == "true") return Shapes::empty();
        std::string f = getProp(props, "facing");
        bool xAxis = (f == "east" || f == "west");
        return xAxis ? Shapes::box(0.375, 0, 0.0, 0.625, 1.5, 1.0) : Shapes::box(0.0, 0, 0.375, 1.0, 1.5, 0.625);
    }
    if (fam == "RedStoneWireBlock") return Shapes::empty();  // noCollision -> empty
    if (fam == "PipeBlock" || fam == "ChorusPlantBlock") {  // core cube(10) + per-face arms boxZ(10,0,8)
        mc::VoxelShapePtr shape = Shapes::box(0.1875, 0.1875, 0.1875, 0.8125, 0.8125, 0.8125);
        if (getProp(props, "north") == "true") shape = Shapes::or_(shape, Shapes::box(0.1875, 0.1875, 0.0, 0.8125, 0.8125, 0.5));
        if (getProp(props, "south") == "true") shape = Shapes::or_(shape, Shapes::box(0.1875, 0.1875, 0.5, 0.8125, 0.8125, 1.0));
        if (getProp(props, "west")  == "true") shape = Shapes::or_(shape, Shapes::box(0.0, 0.1875, 0.1875, 0.5, 0.8125, 0.8125));
        if (getProp(props, "east")  == "true") shape = Shapes::or_(shape, Shapes::box(0.5, 0.1875, 0.1875, 1.0, 0.8125, 0.8125));
        if (getProp(props, "down")  == "true") shape = Shapes::or_(shape, Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.5, 0.8125));
        if (getProp(props, "up")    == "true") shape = Shapes::or_(shape, Shapes::box(0.1875, 0.5, 0.1875, 0.8125, 1.0, 0.8125));
        return shape;
    }
    if (fam == "PointedDripstoneBlock") {  // shape by thickness; .move(getOffset(ZERO)) = (-0.125,0,-0.125)
        std::string th = getProp(props, "thickness");
        mc::VoxelShapePtr base;
        if (th == "tip_merge") base = Shapes::box(0.3125, 0.0, 0.3125, 0.6875, 1.0, 0.6875);
        else if (th == "tip") base = (getProp(props, "vertical_direction") == "down")
            ? Shapes::box(0.3125, 0.3125, 0.3125, 0.6875, 1.0, 0.6875)
            : Shapes::box(0.3125, 0.0, 0.3125, 0.6875, 0.6875, 0.6875);
        else if (th == "frustum") base = Shapes::box(0.25, 0.0, 0.25, 0.75, 1.0, 0.75);       // column(8,0,16)
        else if (th == "middle") base = Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 1.0, 0.8125); // column(10,0,16)
        else base = Shapes::box(0.125, 0.0, 0.125, 0.875, 1.0, 0.875);                          // column(12,0,16)
        return base->move(-0.125, 0.0, -0.125);
    }
    if (fam == "BambooStalkBlock")  // column(3,0,16).move(getOffset(ZERO)) = (-0.25,0,-0.25)
        return Shapes::box(0.40625, 0.0, 0.40625, 0.59375, 1.0, 0.59375)->move(-0.25, 0.0, -0.25);

    // ── agent 1 (sculk / shulker / mossy carpet). Verified box-for-box vs GT TSV. ──
    // SculkSensorBlock + CalibratedSculkSensorBlock: SHAPE = column(16,0,8); getShape (coll==getShape).
    if (fam == "SculkSensorBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.5, 1.0);
    // SculkShriekerBlock.getCollisionShape :78-81 = SHAPE_COLLISION = column(16,0,8).
    if (fam == "SculkShriekerBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.5, 1.0);
    // ShulkerBoxBlock.getShape :151-156 — with empty block-getter (no block entity) -> full cube.
    if (fam == "ShulkerBoxBlock") return Shapes::block();
    // MossyCarpetBlock.getCollisionShape :93-96 = BASE ? shapes(default)=tall.get(DOWN) : empty.
    // BASE comes from prop "bottom"; collision ignores the wall props (visual getShape only).
    if (fam == "MossyCarpetBlock") {
        if (getProp(props, "bottom") == "true") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.0625, 1.0);
        return Shapes::empty();
    }

    // ── agent 3 (candle / amethyst / dripleaf / wall-hanging-sign / pitcher-crop). Verified vs GT TSV. ──
    // CandleBlock.getShape :139-142 (coll==getShape) — SHAPES[candles-1] (:62-67).
    if (fam == "CandleBlock") {
        int candles = std::stoi(getProp(props, "candles"));
        if (candles == 1) return Shapes::box(0.4375, 0.0, 0.4375, 0.5625, 0.375, 0.5625);
        else if (candles == 2) return Shapes::box(0.3125, 0.0, 0.375, 0.6875, 0.375, 0.5625);
        else if (candles == 3) return Shapes::box(0.3125, 0.0, 0.375, 0.625, 0.375, 0.6875);
        else if (candles == 4) return Shapes::box(0.3125, 0.0, 0.3125, 0.6875, 0.375, 0.625);
        return nullptr;
    }
    // CandleCakeBlock.getShape :64-67 — SHAPE :38 = or(cake-base column(14,0,8), candle column(2,8,14)).
    if (fam == "CandleCakeBlock")
        return Shapes::or_(Shapes::box(0.0625, 0.0, 0.0625, 0.9375, 0.5, 0.9375),
                           Shapes::box(0.4375, 0.5, 0.4375, 0.5625, 0.875, 0.5625));
    // AmethystClusterBlock.getShape :52-55 (coll==getShape) — rotateAll(boxZ(width,16-height,16)) by FACING;
    // ctor h/w per variant (Blocks.java): cluster(7,10) large(5,10) medium(4,10) small(3,8).
    if (fam == "AmethystClusterBlock") {
        std::string facing = getProp(props, "facing");
        if (name == "amethyst_cluster") {
            if (facing == "north") return Shapes::box(0.1875, 0.1875, 0.5625, 0.8125, 0.8125, 1.0);
            else if (facing == "east") return Shapes::box(0.0, 0.1875, 0.1875, 0.4375, 0.8125, 0.8125);
            else if (facing == "south") return Shapes::box(0.1875, 0.1875, 0.0, 0.8125, 0.8125, 0.4375);
            else if (facing == "west") return Shapes::box(0.5625, 0.1875, 0.1875, 1.0, 0.8125, 0.8125);
            else if (facing == "up") return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.4375, 0.8125);
            else if (facing == "down") return Shapes::box(0.1875, 0.5625, 0.1875, 0.8125, 1.0, 0.8125);
        } else if (name == "large_amethyst_bud") {
            if (facing == "north") return Shapes::box(0.1875, 0.1875, 0.6875, 0.8125, 0.8125, 1.0);
            else if (facing == "east") return Shapes::box(0.0, 0.1875, 0.1875, 0.3125, 0.8125, 0.8125);
            else if (facing == "south") return Shapes::box(0.1875, 0.1875, 0.0, 0.8125, 0.8125, 0.3125);
            else if (facing == "west") return Shapes::box(0.6875, 0.1875, 0.1875, 1.0, 0.8125, 0.8125);
            else if (facing == "up") return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.3125, 0.8125);
            else if (facing == "down") return Shapes::box(0.1875, 0.6875, 0.1875, 0.8125, 1.0, 0.8125);
        } else if (name == "medium_amethyst_bud") {
            if (facing == "north") return Shapes::box(0.1875, 0.1875, 0.75, 0.8125, 0.8125, 1.0);
            else if (facing == "east") return Shapes::box(0.0, 0.1875, 0.1875, 0.25, 0.8125, 0.8125);
            else if (facing == "south") return Shapes::box(0.1875, 0.1875, 0.0, 0.8125, 0.8125, 0.25);
            else if (facing == "west") return Shapes::box(0.75, 0.1875, 0.1875, 1.0, 0.8125, 0.8125);
            else if (facing == "up") return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.25, 0.8125);
            else if (facing == "down") return Shapes::box(0.1875, 0.75, 0.1875, 0.8125, 1.0, 0.8125);
        } else if (name == "small_amethyst_bud") {
            if (facing == "north") return Shapes::box(0.25, 0.25, 0.8125, 0.75, 0.75, 1.0);
            else if (facing == "east") return Shapes::box(0.0, 0.25, 0.25, 0.1875, 0.75, 0.75);
            else if (facing == "south") return Shapes::box(0.25, 0.25, 0.0, 0.75, 0.75, 0.1875);
            else if (facing == "west") return Shapes::box(0.8125, 0.25, 0.25, 1.0, 0.75, 0.75);
            else if (facing == "up") return Shapes::box(0.25, 0.0, 0.25, 0.75, 0.1875, 0.75);
            else if (facing == "down") return Shapes::box(0.25, 0.8125, 0.25, 0.75, 1.0, 0.75);
        }
        return nullptr;
    }
    // BigDripleafBlock.getCollisionShape :261-264 — SHAPE_LEAF.get(TILT) (:59-70), facing-independent.
    if (fam == "BigDripleafBlock") {
        std::string tilt = getProp(props, "tilt");
        if (tilt == "none" || tilt == "unstable") return Shapes::box(0.0, 0.6875, 0.0, 1.0, 0.9375, 1.0);
        else if (tilt == "partial") return Shapes::box(0.0, 0.6875, 0.0, 1.0, 0.8125, 1.0);
        else if (tilt == "full") return Shapes::empty();
        return nullptr;
    }
    // WallHangingSignBlock.getCollisionShape :97-100 — SHAPES_PLANK.get(FACING.axis) (:44).
    if (fam == "WallHangingSignBlock") {
        std::string facing = getProp(props, "facing");
        if (facing == "north" || facing == "south") return Shapes::box(0.0, 0.875, 0.375, 1.0, 1.0, 0.625);
        else if (facing == "east" || facing == "west") return Shapes::box(0.375, 0.875, 0.0, 0.625, 1.0, 1.0);
        return nullptr;
    }
    // PitcherCropBlock.getCollisionShape :76-83 — UPPER->empty; LOWER age0->SHAPE_BULB else SHAPE_CROP.
    if (fam == "PitcherCropBlock") {
        std::string half = getProp(props, "half");
        if (half == "upper") return Shapes::empty();
        int age = std::stoi(getProp(props, "age"));
        if (age == 0) return Shapes::box(0.3125, -0.0625, 0.3125, 0.6875, 0.1875, 0.6875);
        return Shapes::box(0.1875, -0.0625, 0.1875, 0.8125, 0.3125, 0.8125);
    }

    // ── agent 2 (skulls / shelf / copper-golem statue). getShape == collisionShape (hasCollision). ──
    // SkullBlock.getShape :41-44 — SHAPE=column(8,0,8); SHAPE_PIGLIN=column(10,0,8) (piglin_head).
    if (fam == "SkullBlock") {
        if (name == "piglin_head") return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.5, 0.8125);
        return Shapes::box(0.25, 0.0, 0.25, 0.75, 0.5, 0.75);
    }
    // WallSkullBlock.getShape :35-38 — rotateHorizontal(boxZ(8,8,16)); north base box(4,4,8,12,12,16).
    if (fam == "WallSkullBlock") {
        std::string f = getProp(props, "facing");
        if (f == "north") return Shapes::box(0.25, 0.25, 0.5, 0.75, 0.75, 1.0);
        if (f == "south") return Shapes::box(0.25, 0.25, 0.0, 0.75, 0.75, 0.5);
        if (f == "east")  return Shapes::box(0.0, 0.25, 0.25, 0.5, 0.75, 0.75);
        if (f == "west")  return Shapes::box(0.5, 0.25, 0.25, 1.0, 0.75, 0.75);
        return nullptr;
    }
    // PiglinWallSkullBlock.getShape :27-30 — rotateHorizontal(boxZ(10,8,8,16)); north box(3,4,8,13,12,16).
    if (fam == "PiglinWallSkullBlock") {
        std::string f = getProp(props, "facing");
        if (f == "north") return Shapes::box(0.1875, 0.25, 0.5, 0.8125, 0.75, 1.0);
        if (f == "south") return Shapes::box(0.1875, 0.25, 0.0, 0.8125, 0.75, 0.5);
        if (f == "east")  return Shapes::box(0.0, 0.25, 0.1875, 0.5, 0.75, 0.8125);
        if (f == "west")  return Shapes::box(0.5, 0.25, 0.1875, 1.0, 0.75, 0.8125);
        return nullptr;
    }
    // ShelfBlock.getShape :76-79 — rotateHorizontal(or(box(0,12,11,16,16,13),box(0,0,13,16,16,16),box(0,0,11,16,4,13))).
    if (fam == "ShelfBlock") {
        std::string f = getProp(props, "facing");
        if (f == "north")
            return Shapes::or_(Shapes::or_(Shapes::box(0.0, 0.75, 0.6875, 1.0, 1.0, 0.8125),
                Shapes::box(0.0, 0.0, 0.8125, 1.0, 1.0, 1.0)), Shapes::box(0.0, 0.0, 0.6875, 1.0, 0.25, 0.8125));
        if (f == "south")
            return Shapes::or_(Shapes::or_(Shapes::box(0.0, 0.75, 0.1875, 1.0, 1.0, 0.3125),
                Shapes::box(0.0, 0.0, 0.0, 1.0, 1.0, 0.1875)), Shapes::box(0.0, 0.0, 0.1875, 1.0, 0.25, 0.3125));
        if (f == "east")
            return Shapes::or_(Shapes::or_(Shapes::box(0.1875, 0.75, 0.0, 0.3125, 1.0, 1.0),
                Shapes::box(0.0, 0.0, 0.0, 0.1875, 1.0, 1.0)), Shapes::box(0.1875, 0.0, 0.0, 0.3125, 0.25, 1.0));
        if (f == "west")
            return Shapes::or_(Shapes::or_(Shapes::box(0.6875, 0.75, 0.0, 0.8125, 1.0, 1.0),
                Shapes::box(0.8125, 0.0, 0.0, 1.0, 1.0, 1.0)), Shapes::box(0.6875, 0.0, 0.0, 0.8125, 0.25, 1.0));
        return nullptr;
    }
    // CopperGolemStatueBlock.getShape :92-95 — single SHAPE=column(10,0,14), facing/pose-independent.
    if (fam == "CopperGolemStatueBlock") return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.875, 0.8125);

    // ── stragglers (constant / simple-prop getShape == collisionShape unless noted). ──
    // DiodeBlock.getShape :39-40 — SHAPE=column(16,0,2) (repeater + comparator).
    if (fam == "DiodeBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.125, 1.0);
    // DaylightDetectorBlock.getShape :45-46 — SHAPE=column(16,0,6).
    if (fam == "DaylightDetectorBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.375, 1.0);
    // DriedGhastBlock.getShape :80-81 — SHAPE=column(10,10,0,10).
    if (fam == "DriedGhastBlock") return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 0.625, 0.8125);
    // LightBlock.getShape :66-67 — isHoldingItem(LIGHT)?block:empty; empty CollisionContext -> empty.
    if (fam == "LightBlock") return Shapes::empty();
    // LiquidBlock.getCollisionShape :81-89 — empty CollisionContext: level!=0 / no colliding mob -> empty.
    if (fam == "LiquidBlock") return Shapes::empty();
    // MovingPistonBlock.getCollisionShape :106-108 — null block entity (empty getter) -> empty.
    if (fam == "MovingPistonBlock") return Shapes::empty();
    // CactusBlock.getCollisionShape :85-86 — SHAPE_COLLISION=column(14,0,15) (getShape uses 16; collision 15).
    if (fam == "CactusBlock") return Shapes::box(0.0625, 0.0, 0.0625, 0.9375, 0.9375, 0.9375);
    // FarmlandBlock.getShape :83-84 — SHAPE=column(16,0,15).
    if (fam == "FarmlandBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.9375, 1.0);
    // SnifferEggBlock.getShape :52-53 — SHAPE=column(14,12,0,16) (hatch-independent).
    if (fam == "SnifferEggBlock") return Shapes::box(0.0625, 0.0, 0.125, 0.9375, 1.0, 0.875);
    // AzaleaBlock.getShape :33-34 — SHAPE=or(column(16,8,16), column(4,0,8)).
    if (fam == "AzaleaBlock")
        return Shapes::or_(Shapes::box(0.0, 0.5, 0.0, 1.0, 1.0, 1.0), Shapes::box(0.375, 0.0, 0.375, 0.625, 0.5, 0.625));
    // EndPortalFrameBlock.getShape :50-51 — HAS_EYE ? SHAPE_FULL : SHAPE_EMPTY (:30-31).
    if (fam == "EndPortalFrameBlock") {
        mc::VoxelShapePtr base = Shapes::box(0.0, 0.0, 0.0, 1.0, 0.8125, 1.0);  // column(16,0,13)
        if (getProp(props, "eye") == "true")
            return Shapes::or_(base, Shapes::box(0.25, 0.8125, 0.25, 0.75, 1.0, 0.75));  // +column(8,13,16)
        return base;
    }
    // CocoaBlock.getShape :68-69 — SHAPES.get(age).get(facing) = rotateHorizontal(column(4+2a,7-2a,12)
    // .move(0,0,(a-5)/16)) (:33). Per-state canonical boxes (a∈0..2 × 4 facings) from GT.
    if (fam == "CocoaBlock") {
        int a = std::stoi(getProp(props, "age"));
        std::string f = getProp(props, "facing");
        if (a == 0) {
            if (f == "north") return Shapes::box(0.375, 0.4375, 0.0625, 0.625, 0.75, 0.3125);
            if (f == "south") return Shapes::box(0.375, 0.4375, 0.6875, 0.625, 0.75, 0.9375);
            if (f == "west")  return Shapes::box(0.0625, 0.4375, 0.375, 0.3125, 0.75, 0.625);
            if (f == "east")  return Shapes::box(0.6875, 0.4375, 0.375, 0.9375, 0.75, 0.625);
        } else if (a == 1) {
            if (f == "north") return Shapes::box(0.3125, 0.3125, 0.0625, 0.6875, 0.75, 0.4375);
            if (f == "south") return Shapes::box(0.3125, 0.3125, 0.5625, 0.6875, 0.75, 0.9375);
            if (f == "west")  return Shapes::box(0.0625, 0.3125, 0.3125, 0.4375, 0.75, 0.6875);
            if (f == "east")  return Shapes::box(0.5625, 0.3125, 0.3125, 0.9375, 0.75, 0.6875);
        } else {  // age 2
            if (f == "north") return Shapes::box(0.25, 0.1875, 0.0625, 0.75, 0.75, 0.5625);
            if (f == "south") return Shapes::box(0.25, 0.1875, 0.4375, 0.75, 0.75, 0.9375);
            if (f == "west")  return Shapes::box(0.0625, 0.1875, 0.25, 0.5625, 0.75, 0.75);
            if (f == "east")  return Shapes::box(0.4375, 0.1875, 0.25, 0.9375, 0.75, 0.75);
        }
        return nullptr;
    }
    // PistonBaseBlock.getShape :70-71 — EXTENDED ? SHAPES.get(facing) : block(); SHAPES=rotateAll(boxZ(16,4,16)).
    if (fam == "PistonBaseBlock") {
        if (getProp(props, "extended") != "true") return Shapes::block();
        std::string f = getProp(props, "facing");
        if (f == "north") return Shapes::box(0.0, 0.0, 0.25, 1.0, 1.0, 1.0);
        if (f == "east")  return Shapes::box(0.0, 0.0, 0.0, 0.75, 1.0, 1.0);
        if (f == "south") return Shapes::box(0.0, 0.0, 0.0, 1.0, 1.0, 0.75);
        if (f == "west")  return Shapes::box(0.25, 0.0, 0.0, 1.0, 1.0, 1.0);
        if (f == "up")    return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.75, 1.0);
        return Shapes::box(0.0, 0.25, 0.0, 1.0, 1.0, 1.0);  // down
    }
    // PistonHeadBlock.getShape :60-61 — (SHORT?SHAPES_SHORT:SHAPES).get(facing) = rotateAll(or(
    // platform boxZ(16,0,4), arm boxZ(4,4,SHORT?16:20))) (:40-42). plate + arm per facing; arm far/near
    // end = short?1.0:1.25 / short?0.0:-0.25. or_ canonicalizes to the GT box set (TYPE-independent).
    if (fam == "PistonHeadBlock") {
        std::string f = getProp(props, "facing");
        bool sh = getProp(props, "short") == "true";
        double far = sh ? 1.0 : 1.25, near = sh ? 0.0 : -0.25;
        if (f == "north") return Shapes::or_(Shapes::box(0.0, 0.0, 0.0, 1.0, 1.0, 0.25), Shapes::box(0.375, 0.375, 0.25, 0.625, 0.625, far));
        if (f == "south") return Shapes::or_(Shapes::box(0.0, 0.0, 0.75, 1.0, 1.0, 1.0), Shapes::box(0.375, 0.375, near, 0.625, 0.625, 0.75));
        if (f == "west")  return Shapes::or_(Shapes::box(0.0, 0.0, 0.0, 0.25, 1.0, 1.0), Shapes::box(0.25, 0.375, 0.375, far, 0.625, 0.625));
        if (f == "east")  return Shapes::or_(Shapes::box(0.75, 0.0, 0.0, 1.0, 1.0, 1.0), Shapes::box(near, 0.375, 0.375, 0.75, 0.625, 0.625));
        if (f == "up")    return Shapes::or_(Shapes::box(0.0, 0.75, 0.0, 1.0, 1.0, 1.0), Shapes::box(0.375, near, 0.375, 0.625, 1.0, 0.625));
        return Shapes::or_(Shapes::box(0.0, 0.0, 0.0, 1.0, 0.25, 1.0), Shapes::box(0.375, 0.25, 0.375, 0.625, far, 0.625));  // down
    }

    // ── final single/double-state constants. ──
    // HeavyCoreBlock.getShape :72 — SHAPE=column(8,0,8).
    if (fam == "HeavyCoreBlock") return Shapes::box(0.25, 0.0, 0.25, 0.75, 0.5, 0.75);
    // DirtPathBlock.getShape :73 — SHAPE=column(16,0,15).
    if (fam == "DirtPathBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.9375, 1.0);
    // DragonEggBlock.getShape :34 — SHAPE=column(14,0,16).
    if (fam == "DragonEggBlock") return Shapes::box(0.0625, 0.0, 0.0625, 0.9375, 1.0, 0.9375);
    // HoneyBlock.getCollisionShape :47 — SHAPE=column(14,0,15).
    if (fam == "HoneyBlock") return Shapes::box(0.0625, 0.0, 0.0625, 0.9375, 0.9375, 0.9375);
    // LilyPadBlock.getShape :44 — SHAPE=column(14,0,1.5).
    if (fam == "LilyPadBlock") return Shapes::box(0.0625, 0.0, 0.0625, 0.9375, 0.09375, 0.9375);
    // MudBlock.getCollisionShape :27-28 — SHAPE=column(16,0,14) (block-support/visual are full, collision sunken).
    if (fam == "MudBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.875, 1.0);
    // SoulSandBlock.getCollisionShape :27-28 — SHAPE=column(16,0,14) (block-support/visual are full, collision sunken).
    if (fam == "SoulSandBlock") return Shapes::box(0.0, 0.0, 0.0, 1.0, 0.875, 1.0);
    // PowderSnowBlock.getCollisionShape :116-131 — empty CollisionContext (no entity) -> Shapes.empty().
    if (fam == "PowderSnowBlock") return Shapes::empty();
    return nullptr;
}

inline bool endsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// Block.isShapeFullBlock (Block.java:354-356): SHAPE_FULL_BLOCK_CACHE = !joinIsNotEmpty(block, shape,
// NOT_SAME) — geometric "shape == full unit cube" (XOR with block is empty). getFaceShape returns a
// SliceShape (full unit cube in its normalized coords when the face is fully covered, even with
// internal coordinate splits), so a singleton-pointer compare is insufficient.
bool isShapeFullBlock(const mc::VoxelShapePtr& shape) {
    return !mc::Shapes::joinIsNotEmpty(mc::Shapes::block(), shape, mc::BooleanOps::NOT_SAME);
}
// Block.isFaceFull (Block.java:349-352): isShapeFullBlock(shape.getFaceShape(dir)). The exact
// primitive CrossCollisionBlock/WallBlock.connectsTo consumes (isFaceSturdy with SupportType.FULL).
bool isFaceFull(const mc::VoxelShapePtr& shape, mc::Direction dir) {
    return isShapeFullBlock(shape->getFaceShape(dir));
}

// C++ collision shape as a VoxelShape (mirrors this gate's classification; collision todo==0 so
// always defined). Used as the BlockBehaviour default getBlockSupportShape.
mc::VoxelShapePtr cppCollisionPtr(const std::string& collFam, const std::string& shapeFam,
                                  int hasColl, const std::string& name, const std::string& props) {
    if (collFam == "BlockBehaviour" && hasColl == 0) return mc::Shapes::empty();
    if (collFam == "BlockBehaviour" && shapeFam == "BlockBehaviour")
        return hasColl ? mc::Shapes::block() : mc::Shapes::empty();
    std::string fkey = (shapeFam == "BlockBehaviour" ? collFam : shapeFam);
    return buildFamilyShape(fkey, name, props);
}

// getBlockSupportShape: BlockBehaviour default (BlockBehaviour.java:298) == getCollisionShape
// (== cppColl). 9 classes override it (1:1 from 26.1.2/src; identified by block name).
mc::VoxelShapePtr buildSupportShape(const std::string& name, const std::string& props,
                                    const mc::VoxelShapePtr& cppColl) {
    using mc::Shapes;
    if (name.find("leaves") != std::string::npos) return Shapes::empty();   // LeavesBlock -> empty
    if (name == "mud" || name == "soul_sand") return Shapes::block();        // Mud/SoulSand -> full cube
    if (name == "chorus_flower")                                             // SHAPE_BLOCK_SUPPORT=column(14,0,15)
        return Shapes::box(0.0625, 0.0, 0.0625, 0.9375, 0.9375, 0.9375);
    if (endsWith(name, "shulker_box")) return Shapes::block();               // null block-entity -> block
    if (name == "snow") {                                                    // SHAPES[layers]=column(16,0,layers*2)
        int L = std::stoi(getProp(props, "layers"));
        return Shapes::box(0.0, 0.0, 0.0, 1.0, (L * 2) / 16.0, 1.0);
    }
    if (endsWith(name, "fence_gate")) {                                      // OPEN?empty:SHAPE_SUPPORT[axis]
        if (getProp(props, "open") == "true") return Shapes::empty();        // =rotateHorizontalAxis(column(16,4,5,24))
        std::string f = getProp(props, "facing");
        bool xAxis = (f == "east" || f == "west");
        return xAxis ? Shapes::box(0.375, 0.3125, 0.0, 0.625, 1.5, 1.0)
                     : Shapes::box(0.0, 0.3125, 0.375, 1.0, 1.5, 0.625);
    }
    if (endsWith(name, "wall_hanging_sign")) {                               // getShape=SHAPES[facing.axis]
        std::string f = getProp(props, "facing");                           // =or(SHAPES_PLANK[Z], column(14,2,0,10))
        if (f == "north" || f == "south")
            return Shapes::or_(Shapes::box(0.0, 0.875, 0.375, 1.0, 1.0, 0.625),
                               Shapes::box(0.0625, 0.0, 0.4375, 0.9375, 0.625, 0.5625));
        return Shapes::or_(Shapes::box(0.375, 0.875, 0.0, 0.625, 1.0, 1.0),
                           Shapes::box(0.4375, 0.0, 0.0625, 0.5625, 0.625, 0.9375));
    }
    if (endsWith(name, "hanging_sign")) {                                    // CeilingHangingSign getShape
        int r = std::stoi(getProp(props, "rotation"));                       // SHAPES{0,4,8,12}, else SHAPE_DEFAULT
        if (r == 0 || r == 8) return Shapes::box(0.0625, 0.0, 0.4375, 0.9375, 0.625, 0.5625);
        if (r == 4 || r == 12) return Shapes::box(0.4375, 0.0, 0.0625, 0.5625, 0.625, 0.9375);
        return Shapes::box(0.1875, 0.0, 0.1875, 0.8125, 1.0, 0.8125);        // SHAPE_DEFAULT=column(10,0,16)
    }
    return cppColl;                                                          // default == collision shape
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/block_collision_shape.tsv";
    std::string statesPath = "mcpp/src/assets/block_states.json";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
    }

    // block_states.json -> per-id (short name, is_solid).
    std::vector<std::string> name, props;
    std::vector<int> isSolid;
    {
        std::ifstream f(statesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
        nlohmann::json j; f >> j;
        auto arr = j.at("states");
        name.resize(arr.size()); props.resize(arr.size()); isSolid.resize(arr.size(), 0);
        for (auto& s : arr) {
            std::size_t id = s.at("id").get<std::size_t>();
            name[id] = s.at("name").get<std::string>();
            props[id] = s.value("props", std::string());
            isSolid[id] = s.value("is_solid", false) ? 1 : 0;
        }
    }

    // GT: SHAPE rows -> per-id expected collision AABB list; SUP -> getBlockSupportShape AABB list;
    // STURDY -> the 6 isFaceSturdy booleans; FAM -> block -> (collFam, shapeFam, hasCollision).
    std::vector<Shape> expected(name.size()), expectedSup(name.size());
    std::vector<int> hasShape(name.size(), 0), hasSup(name.size(), 0), hasSturdy(name.size(), 0);
    std::vector<std::array<int,6>> sturdy(name.size());
    std::map<std::string, std::pair<std::string,std::string>> fam;
    std::map<std::string, int> blocksMotion;  // block -> default-state blocksMotion (authoritative hasCollision)
    auto parseBoxes = [](const std::vector<std::string>& c, int n) {
        Shape sh;
        for (int b = 0; b < n; ++b) {
            std::size_t o = 3 + b * 6;
            if (o + 5 >= c.size()) break;
            sh.push_back({ std::stod(c[o]), std::stod(c[o+1]), std::stod(c[o+2]),
                           std::stod(c[o+3]), std::stod(c[o+4]), std::stod(c[o+5]) });
        }
        return sh;
    };
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            auto c = splitTab(line);
            if (c.empty()) continue;
            if (c[0] == "SHAPE" && c.size() >= 3) {
                std::size_t id = (std::size_t)std::stoul(c[1]);
                int n = std::stoi(c[2]);
                if (id >= expected.size() || n < 0) continue;  // n<0 = context-dependent (deferred)
                expected[id] = parseBoxes(c, n); hasShape[id] = 1;
            } else if (c[0] == "SUP" && c.size() >= 3) {
                std::size_t id = (std::size_t)std::stoul(c[1]);
                int n = std::stoi(c[2]);
                if (id >= expectedSup.size() || n < 0) continue;
                expectedSup[id] = parseBoxes(c, n); hasSup[id] = 1;
            } else if (c[0] == "STURDY" && c.size() >= 8) {
                std::size_t id = (std::size_t)std::stoul(c[1]);
                if (id >= sturdy.size()) continue;
                for (int d = 0; d < 6; ++d) sturdy[id][d] = std::stoi(c[2 + d]);
                hasSturdy[id] = 1;
            } else if (c[0] == "FAM" && c.size() >= 4) {
                std::string key = c[1]; auto col = key.find(':'); if (col != std::string::npos) key = key.substr(col + 1);
                fam[key] = { c[2], c[3] };
                if (c.size() >= 5) blocksMotion[key] = std::stoi(c[4]);
            }
        }
    }

    long cert = 0, mis = 0, todo = 0;
    std::map<std::string, long> todoFam;
    int shown = 0;
    for (std::size_t id = 0; id < name.size(); ++id) {
        if (!hasShape[id]) continue;
        auto fi = fam.find(name[id]);
        std::string collFam = fi == fam.end() ? "?" : fi->second.first;
        std::string shapeFam = fi == fam.end() ? "?" : fi->second.second;
        const Shape& want = expected[id];
        int hasColl;
        { auto hcit = blocksMotion.find(name[id]);
          hasColl = (hcit != blocksMotion.end() ? hcit->second : isSolid[id]); }
        // BlockBehaviour.getCollisionShape (:334): a block that does NOT override getCollisionShape
        // (collFam == "BlockBehaviour") yields  hasCollision ? state.getShape() : Shapes.empty().
        // Hence when hasCollision is false the collision shape is EMPTY regardless of any custom
        // getShape — signs/banners/torches/fire/coral/multiface/vines/tripwire/dripleaf-stems/...
        // This is the real default semantics, not a per-family guess (RULE #0).
        if (collFam == "BlockBehaviour" && hasColl == 0) {
            if (want == EMPTY) ++cert;
            else { ++mis; if (shown++ < 16) std::cerr << "mismatch(noColl) id=" << id << " "
                << name[id] << " wantBoxes=" << want.size() << "\n"; }
            continue;
        }
        // Fully-default family whose shape is the cube-or-empty default: collision =
        // hasCollision ? Shapes.block() : Shapes.empty() (BlockBehaviour.getCollisionShape :334,
        // default getShape = full cube). Blocks that set a CUSTOM shape field via the constructor
        // (without overriding getShape, e.g. chorus_flower) yield a non-cube box -> fan-out.
        bool defaultFamily = (collFam == "BlockBehaviour" && shapeFam == "BlockBehaviour");
        bool cubeOrEmpty = (want == CUBE || want == EMPTY);
        if (defaultFamily && cubeOrEmpty) {
            auto hc = blocksMotion.find(name[id]);
            const Shape& got = (hc != blocksMotion.end() ? hc->second : isSolid[id]) ? CUBE : EMPTY;
            if (got == want) ++cert;
            else {
                ++mis;
                if (shown++ < 16) std::cerr << "mismatch id=" << id << " " << name[id]
                    << " hasCollision=" << (hc != blocksMotion.end() ? hc->second : -1)
                    << " gotBoxes=" << got.size() << " wantBoxes=" << want.size() << "\n";
            }
            continue;
        }
        // Per-family getShape (Shapes-composition) for the custom-shape families.
        std::string fkey = (shapeFam == "BlockBehaviour" ? collFam : shapeFam);
        if (mc::VoxelShapePtr sh = buildFamilyShape(fkey, name[id], props[id])) {
            Shape got = toBoxes(sh);
            // GT expected is already canonical-sorted; sort ours too (toBoxes sorts).
            Shape wantSorted = expected[id];
            std::sort(wantSorted.begin(), wantSorted.end(), [](const Box& p, const Box& q){
                return std::tie(p.x1,p.y1,p.z1,p.x2,p.y2,p.z2) < std::tie(q.x1,q.y1,q.z1,q.x2,q.y2,q.z2); });
            if (got == wantSorted) ++cert;
            else {
                ++mis;
                if (shown++ < 16) std::cerr << "mismatch id=" << id << " " << name[id] << "[" << props[id]
                    << "] fam=" << fkey << " gotBoxes=" << got.size() << " wantBoxes=" << wantSorted.size() << "\n";
            }
        } else {
            ++todo;
            todoFam[!defaultFamily ? fkey : "<default-custom-box>"]++;
        }
    }

    std::cout << "BlockCollisionShape certified=" << cert << " mismatches=" << mis << " todo=" << todo << "\n";
    std::vector<std::pair<std::string,long>> tv(todoFam.begin(), todoFam.end());
    std::sort(tv.begin(), tv.end(), [](auto&a,auto&b){ return a.second > b.second; });
    std::cout << "remaining shape families (states): ";
    for (std::size_t i = 0; i < tv.size() && i < 14; ++i) std::cout << tv[i].first << "=" << tv[i].second << " ";
    std::cout << "\n";

    // ── getBlockSupportShape (== getCollisionShape default + 9 overrides) and the 6 isFaceSturdy
    // booleans (= Block.isFaceFull(supportShape, dir)), the exact primitive connectsTo consumes. ──
    long supCert = 0, supMis = 0, sturdyCert = 0, sturdyMis = 0;
    std::map<std::string, long> supBadFam;
    int shown2 = 0;
    for (std::size_t id = 0; id < name.size(); ++id) {
        if (!hasSup[id]) continue;
        auto fi = fam.find(name[id]);
        std::string collFam = fi == fam.end() ? "?" : fi->second.first;
        std::string shapeFam = fi == fam.end() ? "?" : fi->second.second;
        int hasColl; { auto it = blocksMotion.find(name[id]);
                       hasColl = (it != blocksMotion.end() ? it->second : isSolid[id]); }
        mc::VoxelShapePtr coll = cppCollisionPtr(collFam, shapeFam, hasColl, name[id], props[id]);
        mc::VoxelShapePtr sup = coll ? buildSupportShape(name[id], props[id], coll) : nullptr;
        if (!sup) { ++supMis; supBadFam[shapeFam]++; continue; }
        Shape got = toBoxes(sup);
        Shape want = expectedSup[id];
        std::sort(want.begin(), want.end(), [](const Box& p, const Box& q){
            return std::tie(p.x1,p.y1,p.z1,p.x2,p.y2,p.z2) < std::tie(q.x1,q.y1,q.z1,q.x2,q.y2,q.z2); });
        if (got == want) ++supCert;
        else { ++supMis; supBadFam[shapeFam]++;
            if (shown2++ < 16) std::cerr << "SUP mismatch " << name[id] << "[" << props[id]
                << "] got=" << got.size() << " want=" << want.size() << "\n"; }
        if (hasSturdy[id]) {
            bool ok = true;
            for (int d = 0; d < 6; ++d)
                if ((isFaceFull(sup, static_cast<mc::Direction>(d)) ? 1 : 0) != sturdy[id][d]) ok = false;
            if (ok) ++sturdyCert;
            else { ++sturdyMis;
                if (shown2++ < 16) { std::cerr << "STURDY mismatch " << name[id] << "[" << props[id] << "] cpp=";
                    for (int d = 0; d < 6; ++d) std::cerr << (isFaceFull(sup, static_cast<mc::Direction>(d)) ? 1 : 0);
                    std::cerr << " want="; for (int d = 0; d < 6; ++d) std::cerr << sturdy[id][d]; std::cerr << "\n"; } }
        }
    }
    std::cout << "BlockSupportShape supCert=" << supCert << " supMismatches=" << supMis
              << " | isFaceSturdy sturdyCert=" << sturdyCert << " sturdyMismatches=" << sturdyMis << "\n";
    if (supMis > 0) {
        std::vector<std::pair<std::string,long>> sv(supBadFam.begin(), supBadFam.end());
        std::sort(sv.begin(), sv.end(), [](auto&a,auto&b){ return a.second > b.second; });
        std::cout << "support-shape mismatch families: ";
        for (std::size_t i = 0; i < sv.size() && i < 14; ++i) std::cout << sv[i].first << "=" << sv[i].second << " ";
        std::cout << "\n";
    }
    return (mis > 0 || supMis > 0 || sturdyMis > 0) ? 1 : 0;
}
