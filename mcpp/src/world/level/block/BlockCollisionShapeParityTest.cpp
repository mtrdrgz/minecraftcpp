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

#include <nlohmann/json.hpp>

#include <algorithm>
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
    return nullptr;
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

    // GT: SHAPE rows -> per-id expected AABB list; FAM rows -> block -> (collFam, shapeFam).
    std::vector<Shape> expected(name.size());
    std::vector<int> hasShape(name.size(), 0);
    std::map<std::string, std::pair<std::string,std::string>> fam;
    std::map<std::string, int> blocksMotion;  // block -> default-state blocksMotion (authoritative hasCollision)
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
                Shape sh;
                for (int b = 0; b < n; ++b) {
                    std::size_t o = 3 + b * 6;
                    if (o + 5 >= c.size()) break;
                    sh.push_back({ std::stod(c[o]), std::stod(c[o+1]), std::stod(c[o+2]),
                                   std::stod(c[o+3]), std::stod(c[o+4]), std::stod(c[o+5]) });
                }
                expected[id] = std::move(sh); hasShape[id] = 1;
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
    return mis > 0 ? 1 : 0;
}
