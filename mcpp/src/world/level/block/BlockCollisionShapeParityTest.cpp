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
mc::VoxelShapePtr buildFamilyShape(const std::string& fam, const std::string& props) {
    using mc::Shapes;
    // SlabBlock.getShape :59-65 — TOP/BOTTOM half-boxes, DOUBLE = full cube.
    if (fam == "SlabBlock") {
        std::string type = getProp(props, "type");
        if (type == "double") return Shapes::block();
        if (type == "top") return Shapes::box(0, 0.5, 0, 1, 1, 1);
        return Shapes::box(0, 0, 0, 1, 0.5, 1);  // bottom
    }
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
        if (mc::VoxelShapePtr sh = buildFamilyShape(fkey, props[id])) {
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
