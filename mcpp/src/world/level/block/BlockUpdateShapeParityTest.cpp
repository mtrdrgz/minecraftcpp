// Parity gate for net.minecraft...Block.updateFromNeighbourShapes — the per-block updateShape
// connection recompute (runs at the placeInWorld knownShape=false tail and on every neighbour
// update). Ground truth: tools/BlockUpdateShapeParity.java drives the REAL
// updateFromNeighbourShapes over real states x controlled 3x3x3 neighbourhoods and emits
// (centreStateId, 26 neighbour ids, outStateId). We replay the SAME neighbourhood and compare.
//
// Certified FAMILY-BY-FAMILY (updateShape declaring class). Unported families are counted as
// `todo` (a printed worklist), NEVER silently passed (RULE #0): a family is only compared once
// its updateShape is ported here.
//
//   block_update_shape_parity [--cases mcpp/build/block_update_shape.tsv]
//                             [--states mcpp/src/assets/block_states.json]

#include "../../phys/shapes/Shapes.h"
#include "../../phys/shapes/VoxelShape.h"
#include "../../phys/shapes/BooleanOp.h"
#include "../../phys/Direction.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    if (!o.empty() && !o.back().empty() && o.back().back() == '\r') o.back().pop_back();
    return o;
}

// ── state table (block_states.json) + (name,props)->id reverse index for setValue. ──
std::vector<std::string> g_name, g_props;
std::unordered_map<std::string, int> g_index;  // "name\x1fprops" -> id
// certified isFaceSturdy[stateId][dir] (Direction order DOWN,UP,NORTH,SOUTH,WEST,EAST), loaded from
// the STURDY rows of block_collision_shape.tsv (block_collision_shape_parity proves it byte-exact).
std::vector<std::array<int,6>> g_sturdy;
std::vector<int> g_hasSturdy;

// ── tag membership (resolved from 26.1.2/data, nested-tag aware) keyed by block name (no ns). ──
// Tags needed by the connection families' connectsTo / attachsTo / isExceptionForConnection /
// shouldRaisePost / isSameFence: WALLS, FENCES, WOODEN_FENCES, BARS, WALL_POST_OVERRIDE,
// SHULKER_BOXES, LEAVES (proxy for instanceof LeavesBlock — every LeavesBlock IS in #leaves).
struct TagSets {
    std::unordered_set<std::string> walls, fences, woodenFences, bars, wallPostOverride,
        shulkerBoxes, leaves, supportsVegetation, supportsCocoa, wallHangingSigns, supportsBigDripleaf,
        supportsHangingMangrove, supportsMangrovePropagule, supportsSmallDripleaf, snow;
};
TagSets g_tags;
// block name (no minecraft:) -> updateShape declaring class (FAM rows). Used to detect
// instanceof IronBarsBlock / FenceGateBlock for any neighbour (the only faithful signal here).
std::map<std::string, std::string> g_updFam;

std::string stripNs(const std::string& s) {
    auto c = s.find(':');
    return c == std::string::npos ? s : s.substr(c + 1);
}
const std::string& famOf(const std::string& name) {
    static const std::string none = "?";
    auto it = g_updFam.find(name);
    return it == g_updFam.end() ? none : it->second;
}

// ── certified collision shapes per state (SHAPE rows of block_collision_shape.tsv). Built lazily
// into the certified VoxelShape primitives so WallBlock's isCovered (getFaceShape + joinIsNotEmpty)
// is byte-faithful. The TSV stores canonical AABB lists in [0,1] block space. ──
std::vector<std::vector<std::array<double,6>>> g_collBoxes;  // per stateId
std::vector<int> g_hasColl;
std::vector<mc::VoxelShapePtr> g_collCache;  // memoised VoxelShape per stateId
std::vector<char> g_collBuilt;
std::vector<std::vector<std::array<double,6>>> g_supBoxes;  // getBlockSupportShape AABBs (SUP rows)
std::vector<int> g_solid;  // block_states.json is_solid (== BlockState.isSolid())

mc::VoxelShapePtr collisionShapeOf(int stateId) {
    if (stateId < 0 || stateId >= (int)g_collBoxes.size()) return mc::Shapes::empty();
    if (g_collBuilt[stateId]) return g_collCache[stateId];
    mc::VoxelShapePtr shape = mc::Shapes::empty();
    for (const auto& b : g_collBoxes[stateId])
        shape = mc::Shapes::or_(shape, mc::Shapes::box(b[0], b[1], b[2], b[3], b[4], b[5]));
    g_collCache[stateId] = shape;
    g_collBuilt[stateId] = 1;
    return shape;
}

std::string getProp(const std::string& props, const std::string& key) {
    std::istringstream ss(props); std::string p;
    while (std::getline(ss, p, ',')) {
        auto eq = p.find('=');
        if (eq != std::string::npos && p.substr(0, eq) == key) return p.substr(eq + 1);
    }
    return "";
}
// state.setValue(key, val): rebuild the props string (state-definition order preserved) and look
// up the resulting state id. Returns -1 if the (name,props') combination is not a real state.
int setProp(int stateId, const std::string& key, const std::string& val) {
    const std::string& props = g_props[stateId];
    std::string out; std::istringstream ss(props); std::string p; bool found = false;
    while (std::getline(ss, p, ',')) {
        auto eq = p.find('=');
        if (!out.empty()) out += ',';
        if (eq != std::string::npos && p.substr(0, eq) == key) { out += key + "=" + val; found = true; }
        else out += p;
    }
    if (!found) return -1;
    auto it = g_index.find(g_name[stateId] + "\x1f" + out);
    return it == g_index.end() ? -1 : it->second;
}

// ── directions (mc Direction order: DOWN,UP,NORTH,SOUTH,WEST,EAST). ──
enum Dir { DOWN = 0, UP, NORTH, SOUTH, WEST, EAST };
const int DX[6] = {0, 0, 0, 0, -1, 1};
const int DY[6] = {-1, 1, 0, 0, 0, 0};
const int DZ[6] = {0, 0, -1, 1, 0, 0};
const int UPDATE_SHAPE_ORDER[6] = {WEST, EAST, NORTH, SOUTH, DOWN, UP};
int opposite(int d) { static const int O[6] = {UP, DOWN, SOUTH, NORTH, EAST, WEST}; return O[d]; }
bool isHorizontal(int d) { return d >= NORTH; }
// getCounterClockWise (around Y): NORTH->WEST->SOUTH->EAST->NORTH.
int counterClockWise(int d) {
    switch (d) { case NORTH: return WEST; case WEST: return SOUTH; case SOUTH: return EAST;
                 case EAST: return NORTH; default: return d; }
}
// getClockWise (around Y): NORTH->EAST->SOUTH->WEST->NORTH.
int clockWise(int d) {
    switch (d) { case NORTH: return EAST; case EAST: return SOUTH; case SOUTH: return WEST;
                 case WEST: return NORTH; default: return d; }
}
int dirFromName(const std::string& s) {
    if (s == "down") return DOWN; if (s == "up") return UP; if (s == "north") return NORTH;
    if (s == "south") return SOUTH; if (s == "west") return WEST; return EAST;
}
// neighbour.isFaceSturdy(level, pos, dir) — state-only (support shape is state-only); certified.
bool isFaceSturdy(int stateId, int dir) {
    return stateId >= 0 && stateId < (int)g_sturdy.size() && g_hasSturdy[stateId] && g_sturdy[stateId][dir];
}

// neighbourhood: offset (dx,dy,dz) in [-1,1]^3 -> stateId; air (0) outside.
struct Level {
    const std::array<int, 27>* cells;  // indexed (dx+1)*9+(dy+1)*3+(dz+1)
    int at(int dx, int dy, int dz) const {
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || dz < -1 || dz > 1) return 0;  // air
        return (*cells)[(dx + 1) * 9 + (dy + 1) * 3 + (dz + 1)];
    }
    int rel(int d) const { return at(DX[d], DY[d], DZ[d]); }  // getBlockState(pos.relative(d))
};

bool isStairsName(const std::string& n) {
    return n.size() >= 6 && n.compare(n.size() - 6, 6, "stairs") == 0;
}
bool endsWith(const std::string& n, const std::string& suf) {
    return n.size() >= suf.size() && n.compare(n.size() - suf.size(), suf.size(), suf) == 0;
}
// neighbourState.getBlock() instanceof DoorBlock — every "_door" block is a DoorBlock.
bool isDoorName(const std::string& n) { return endsWith(n, "_door"); }
// neighbourState.is(this) for a bed — same block (same name as the centre bed).
// chestCanConnectTo(neighbourState)==blockState.is(this) for plain ChestBlock — same block.
// CopperChestBlock.chestCanConnectTo = neighbour is in the COPPER_CHESTS tag and has TYPE.
bool isCopperChestName(const std::string& n) {
    return n == "copper_chest" || n == "exposed_copper_chest" || n == "weathered_copper_chest"
        || n == "oxidized_copper_chest" || n == "waxed_copper_chest" || n == "waxed_exposed_copper_chest"
        || n == "waxed_weathered_copper_chest" || n == "waxed_oxidized_copper_chest";
}
// ChestType.getOpposite (:23-27): SINGLE->SINGLE, LEFT->RIGHT, RIGHT->LEFT.
std::string chestTypeOpposite(const std::string& t) {
    if (t == "left") return "right";
    if (t == "right") return "left";
    return "single";
}
// ChestBlock.getConnectedDirection (:199-202): TYPE==LEFT ? facing.getClockWise() : facing.getCounterClockWise().
int chestConnectedDirection(int stateId) {
    int facing = dirFromName(getProp(g_props[stateId], "facing"));
    return getProp(g_props[stateId], "type") == "left" ? clockWise(facing) : counterClockWise(facing);
}
// Block.withPropertiesOf for a copper-chest re-color: same (facing,type,waterlogged) props, neighbour's block name.
int withChestBlockOf(const std::string& neighbourName, int srcStateId) {
    auto it = g_index.find(neighbourName + "\x1f" + g_props[srcStateId]);
    return it == g_index.end() ? -1 : it->second;
}

// ════════════ CONNECTION FAMILIES (Wall / Fence / IronBars / FenceGate) ════════════
// Ported verbatim from net.minecraft...{WallBlock,CrossCollisionBlock,FenceBlock,IronBarsBlock,
// FenceGateBlock} + Block.isExceptionForConnection. Tag/instanceof checks use the resolved tag
// sets (g_tags) and the FAM declaring class (famOf). State = (name, props) indexed by stateId.

bool inTag(const std::unordered_set<std::string>& tag, int stateId) {
    return tag.count(g_name[stateId]) != 0;
}
// Block.isExceptionForConnection :255-263 — LeavesBlock || BARRIER || CARVED_PUMPKIN ||
// JACK_O_LANTERN || MELON || PUMPKIN || #shulker_boxes. (#leaves == all LeavesBlock instances.)
bool isExceptionForConnection(int stateId) {
    const std::string& n = g_name[stateId];
    return inTag(g_tags.leaves, stateId) || n == "barrier" || n == "carved_pumpkin"
        || n == "jack_o_lantern" || n == "melon" || n == "pumpkin"
        || inTag(g_tags.shulkerBoxes, stateId);
}
bool isIronBarsBlock(int stateId) { return famOf(g_name[stateId]) == "IronBarsBlock"; }
bool isFenceGateBlock(int stateId) { return famOf(g_name[stateId]) == "FenceGateBlock"; }
bool isWallBlock(int stateId) { return famOf(g_name[stateId]) == "WallBlock"; }

// FenceGateBlock.connectsToDirection :213-215 — FACING.getAxis() == direction.getClockWise().getAxis().
bool fenceGateConnectsToDirection(int stateId, int direction) {
    int facing = dirFromName(getProp(g_props[stateId], "facing"));
    auto axisOf = [](int d){ return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); };  // Y=1,Z=2,X=0
    return axisOf(facing) == axisOf(clockWise(direction));
}

// WallBlock.connectsTo :105-109.
bool wallConnectsTo(int stateId, bool faceSolid, int direction) {
    bool connectedFenceGate = isFenceGateBlock(stateId) && fenceGateConnectsToDirection(stateId, direction);
    return inTag(g_tags.walls, stateId)
        || (!isExceptionForConnection(stateId) && faceSolid)
        || isIronBarsBlock(stateId) || connectedFenceGate;
}
// FenceBlock.isSameFence :66-68 — #fences && (#wooden_fences == centre is #wooden_fences).
bool fenceIsSameFence(int neighbourId, int centreId) {
    return inTag(g_tags.fences, neighbourId)
        && (inTag(g_tags.woodenFences, neighbourId) == inTag(g_tags.woodenFences, centreId));
}
// FenceBlock.connectsTo :59-64.
bool fenceConnectsTo(int neighbourId, int centreId, bool faceSolid, int direction) {
    bool sameFence = fenceIsSameFence(neighbourId, centreId);
    bool gate = isFenceGateBlock(neighbourId) && fenceGateConnectsToDirection(neighbourId, direction);
    return (!isExceptionForConnection(neighbourId) && faceSolid) || sameFence || gate;
}
// IronBarsBlock.attachsTo :101-103.
bool barsAttachsTo(int neighbourId, bool faceSolid) {
    return (!isExceptionForConnection(neighbourId) && faceSolid)
        || isIronBarsBlock(neighbourId) || inTag(g_tags.walls, neighbourId);
}

// ── WallBlock isCovered tests (constant shapes; Block.column/boxZ /16 + rotateHorizontal). ──
// TEST_SHAPE_POST = Block.column(2,0,16) -> box(7,0,7,9,16,9)/16.
const mc::VoxelShapePtr& testShapePost() {
    static const mc::VoxelShapePtr s = mc::Shapes::box(0.4375, 0.0, 0.4375, 0.5625, 1.0, 0.5625);
    return s;
}
// TEST_SHAPES_WALL = rotateHorizontal(Block.boxZ(2,16,0,9)); north base box(7,0,0,9,16,9)/16.
// rotateHorizontal direction convention matches the certified WallBlock collision arms
// (BlockCollisionShapeParityTest WallBlock: north z 0..ext, south z (1-ext)..1, west/east on x).
const mc::VoxelShapePtr& testShapeWall(int dir) {
    using mc::Shapes;
    static const mc::VoxelShapePtr north = Shapes::box(0.4375, 0.0, 0.0,    0.5625, 1.0, 0.5625);
    static const mc::VoxelShapePtr south = Shapes::box(0.4375, 0.0, 0.4375, 0.5625, 1.0, 1.0);
    static const mc::VoxelShapePtr west  = Shapes::box(0.0,    0.0, 0.4375, 0.5625, 1.0, 0.5625);
    static const mc::VoxelShapePtr east  = Shapes::box(0.4375, 0.0, 0.4375, 1.0,    1.0, 0.5625);
    switch (dir) { case NORTH: return north; case SOUTH: return south;
                   case WEST: return west; default: return east; }
}
// WallBlock.isCovered :162-164 — !Shapes.joinIsNotEmpty(testShape, aboveShape, ONLY_FIRST).
bool wallIsCovered(const mc::VoxelShapePtr& aboveShape, const mc::VoxelShapePtr& testShape) {
    return !mc::Shapes::joinIsNotEmpty(testShape, aboveShape, mc::BooleanOps::ONLY_FIRST);
}
// WallBlock.makeWallState :247-253 — "none"/"low"/"tall".
std::string wallMakeWallState(bool connectsToSide, const mc::VoxelShapePtr& aboveShape, int dir) {
    if (!connectsToSide) return "none";
    return wallIsCovered(aboveShape, testShapeWall(dir)) ? "tall" : "low";
}
// WallBlock.shouldRaisePost :210-231.
bool wallShouldRaisePost(const std::string& north, const std::string& east, const std::string& south,
                         const std::string& west, int topNeighbourId,
                         const mc::VoxelShapePtr& aboveShape) {
    bool topNeighbourHasPost = isWallBlock(topNeighbourId)
        && getProp(g_props[topNeighbourId], "up") == "true";
    if (topNeighbourHasPost) return true;

    bool southNone = south == "none", westNone = west == "none";
    bool eastNone = east == "none", northNone = north == "none";
    bool hasCorner = (northNone && southNone && westNone && eastNone)
        || (northNone != southNone) || (westNone != eastNone);
    if (hasCorner) return true;

    bool hasHighWall = (north == "tall" && south == "tall") || (east == "tall" && west == "tall");
    if (hasHighWall) return false;
    return inTag(g_tags.wallPostOverride, topNeighbourId)
        || wallIsCovered(aboveShape, testShapePost());
}
// WallBlock private updateShape(level,state,topPos,topNeighbour,n,e,s,w) :195-208 + updateSides
// :233-245 — recompute the 4 WallSides + UP. Returns the new stateId (or stateId if a setProp miss).
int wallApply(int stateId, bool north, bool east, bool south, bool west, int topNeighbourId) {
    mc::VoxelShapePtr aboveShape = collisionShapeOf(topNeighbourId)->getFaceShape(mc::Direction::DOWN);
    std::string ns = wallMakeWallState(north, aboveShape, NORTH);
    std::string es = wallMakeWallState(east,  aboveShape, EAST);
    std::string ss = wallMakeWallState(south, aboveShape, SOUTH);
    std::string ws = wallMakeWallState(west,  aboveShape, WEST);
    int cur = stateId, t;
    t = setProp(cur, "north", ns); if (t < 0) return stateId; cur = t;
    t = setProp(cur, "east",  es); if (t < 0) return stateId; cur = t;
    t = setProp(cur, "south", ss); if (t < 0) return stateId; cur = t;
    t = setProp(cur, "west",  ws); if (t < 0) return stateId; cur = t;
    bool up = wallShouldRaisePost(ns, es, ss, ws, topNeighbourId, aboveShape);
    t = setProp(cur, "up", up ? "true" : "false"); if (t < 0) return stateId;
    return t;
}
// WallBlock.isConnected :158-160 — the side WallSide is not NONE.
bool wallIsConnected(int stateId, const char* sideKey) {
    return getProp(g_props[stateId], sideKey) != "none";
}

// StairBlock.getStairsShape :129-156 — recompute SHAPE from the stair in front/behind.
std::string getStairsShape(int stateId, const Level& level) {
    int facing = dirFromName(getProp(g_props[stateId], "facing"));
    std::string half = getProp(g_props[stateId], "half");
    auto canTakeShape = [&](int nDir) {
        int nb = level.rel(nDir);
        return !isStairsName(g_name[nb]) || dirFromName(getProp(g_props[nb], "facing")) != facing
            || getProp(g_props[nb], "half") != half;
    };
    int behind = level.rel(facing);
    if (isStairsName(g_name[behind]) && getProp(g_props[behind], "half") == half) {
        int bf = dirFromName(getProp(g_props[behind], "facing"));
        bool bfAxisV = (bf == DOWN || bf == UP), fAxisV = false;  // facing is horizontal
        auto axisOf = [](int d){ return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); };
        if (axisOf(bf) != axisOf(facing) && canTakeShape(opposite(bf)))
            return bf == counterClockWise(facing) ? "outer_left" : "outer_right";
    }
    int front = level.rel(opposite(facing));
    if (isStairsName(g_name[front]) && getProp(g_props[front], "half") == half) {
        int ff = dirFromName(getProp(g_props[front], "facing"));
        auto axisOf = [](int d){ return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); };
        if (axisOf(ff) != axisOf(facing) && canTakeShape(ff))
            return ff == counterClockWise(facing) ? "inner_left" : "inner_right";
    }
    return "straight";
}

// ════════════ support-shape & attachment primitives (agents C + D), all certified state-only ════════════
// Direction.getAxis(): Y=1, Z=2, X=0.
int axisOf(int d) { return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); }
// Direction.Plane.HORIZONTAL iteration order {NORTH, EAST, SOUTH, WEST}.
const int HORIZONTAL[4] = {NORTH, EAST, SOUTH, WEST};
std::string horizProp(int d) {
    switch (d) { case NORTH: return "north"; case SOUTH: return "south";
                 case WEST: return "west"; default: return "east"; }
}
const char* faceProp(int d) {  // PipeBlock.PROPERTY_BY_DIRECTION (all 6)
    switch (d) { case DOWN: return "down"; case UP: return "up"; case NORTH: return "north";
                 case SOUTH: return "south"; case WEST: return "west"; default: return "east"; }
}
// BlockState.isSolid() (block_states.json is_solid / legacySolid).
bool isSolidState(int stateId) { return stateId >= 0 && stateId < (int)g_solid.size() && g_solid[stateId]; }

// Block.isFaceFull(shape, dir): the dir-face shape covers [lo,hi]^2 (FULL=0..1; CENTER=7/16..9/16).
// Validated byte-exact vs the certified STURDY column over all 179238 (state*dir) checks (agent C).
bool coverRects(const std::vector<std::array<double,4>>& r, double lo, double hi) {
    if (r.empty()) return false;
    std::vector<double> xs{lo, hi}, ys{lo, hi};
    for (auto& q : r) { xs.push_back(q[0]); xs.push_back(q[2]); ys.push_back(q[1]); ys.push_back(q[3]); }
    std::sort(xs.begin(), xs.end()); xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end()); ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        for (std::size_t k = 0; k + 1 < ys.size(); ++k) {
            double cx = (xs[i] + xs[i+1]) / 2, cy = (ys[k] + ys[k+1]) / 2;
            if (cx < lo || cx > hi || cy < lo || cy > hi) continue;
            bool covered = false;
            for (auto& q : r) if (q[0] <= cx && cx <= q[2] && q[1] <= cy && cy <= q[3]) { covered = true; break; }
            if (!covered) return false;
        }
    return true;
}
std::vector<std::array<double,4>> faceRects(const std::vector<std::array<double,6>>& boxes, int d) {
    // VoxelShape.getFaceShape(d): the slice at the dir-extreme coordinate (findIndex at 0.9999999 /
    // 1.0E-7), projected to the perpendicular plane. A box contributes if it CONTAINS that slice
    // coordinate (not merely touches the [0,1] boundary) — fences/walls extend to y=1.5, so their
    // post is in the UP slice even though its max y != 1.0.
    const double hi = 0.9999999, lo = 1.0E-7;
    std::vector<std::array<double,4>> r;
    for (auto& b : boxes) {  // b = {x0,y0,z0,x1,y1,z1}
        if      (d == DOWN)  { if (b[1] <= lo && lo <= b[4]) r.push_back({b[0], b[2], b[3], b[5]}); }
        else if (d == UP)    { if (b[1] <= hi && hi <= b[4]) r.push_back({b[0], b[2], b[3], b[5]}); }
        else if (d == NORTH) { if (b[2] <= lo && lo <= b[5]) r.push_back({b[0], b[1], b[3], b[4]}); }
        else if (d == SOUTH) { if (b[2] <= hi && hi <= b[5]) r.push_back({b[0], b[1], b[3], b[4]}); }
        else if (d == WEST)  { if (b[0] <= lo && lo <= b[3]) r.push_back({b[1], b[2], b[4], b[5]}); }
        else if (d == EAST)  { if (b[0] <= hi && hi <= b[3]) r.push_back({b[1], b[2], b[4], b[5]}); }
    }
    return r;
}
bool faceFullSup(int id, int d) { return id >= 0 && id < (int)g_supBoxes.size() && coverRects(faceRects(g_supBoxes[id], d), 0.0, 1.0); }
bool faceFullCol(int id, int d) { return id >= 0 && id < (int)g_collBoxes.size() && coverRects(faceRects(g_collBoxes[id], d), 0.0, 1.0); }
bool isCenterSupport(int id, int d) { return id >= 0 && id < (int)g_supBoxes.size() && coverRects(faceRects(g_supBoxes[id], d), 7.0/16.0, 9.0/16.0); }
// SupportType.RIGID (SupportType.java:26-33): face covers the outer ring (block minus column(12,0,16)),
// i.e. the 2px border frame x<2/16||x>14/16||z<2/16||z>14/16. canSupportRigidBlock = RIGID on UP.
bool isRigidSupport(int id, int d) {
    if (id < 0 || id >= (int)g_supBoxes.size()) return false;
    auto r = faceRects(g_supBoxes[id], d);
    if (r.empty()) return false;
    const double a = 2.0 / 16.0, b = 14.0 / 16.0;
    std::vector<double> xs{0.0, 1.0, a, b}, ys{0.0, 1.0, a, b};
    for (auto& q : r) { xs.push_back(q[0]); xs.push_back(q[2]); ys.push_back(q[1]); ys.push_back(q[3]); }
    std::sort(xs.begin(), xs.end()); xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end()); ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    for (std::size_t i = 0; i + 1 < xs.size(); ++i)
        for (std::size_t k = 0; k + 1 < ys.size(); ++k) {
            double cx = (xs[i] + xs[i+1]) / 2, cy = (ys[k] + ys[k+1]) / 2;
            if (cx < 0 || cx > 1 || cy < 0 || cy > 1) continue;
            bool inRing = (cx < a || cx > b || cy < a || cy > b);  // outer 2px border
            if (!inRing) continue;
            bool covered = false;
            for (auto& q : r) if (q[0] <= cx && cx <= q[2] && q[1] <= cy && cy <= q[3]) { covered = true; break; }
            if (!covered) return false;
        }
    return true;
}
// MultifaceBlock.canAttachTo (:256): isFaceFull(getBlockSupportShape, opp) || isFaceFull(collision, opp).
bool multifaceCanAttachTo(int neighbourId, int dirToNeighbour) {
    int opp = opposite(dirToNeighbour);
    return faceFullSup(neighbourId, opp) || faceFullCol(neighbourId, opp);
}
bool canAttachTo(int nbrId, int dirToNbr) { return multifaceCanAttachTo(nbrId, dirToNbr); }
int faceConnectedDir(int stateId) {  // FaceAttachedHorizontalDirectionalBlock.getConnectedDirection
    std::string face = getProp(g_props[stateId], "face");
    if (face == "ceiling") return DOWN;
    if (face == "floor")   return UP;
    return dirFromName(getProp(g_props[stateId], "facing"));
}
bool mfHasFace(int stateId, int d) { return getProp(g_props[stateId], faceProp(d)) == "true"; }
bool mfHasAnyFace(int stateId) { for (int d = 0; d < 6; ++d) if (mfHasFace(stateId, d)) return true; return false; }

// EnumProperty<RedstoneSide>.isConnected(): anything but NONE.
bool sideConnected(const std::string& v) { return !v.empty() && v != "none"; }
// neighbour.isRedstoneConductor (BlockBehaviour.java:1003) == isCollisionShapeFullBlock; glass
// overrides it to never (Blocks.java:619) despite a full cube.
bool isCollisionFullBlock(int id) {
    return id >= 0 && id < (int)g_sturdy.size() && g_hasSturdy[id]
        && g_sturdy[id][0] && g_sturdy[id][1] && g_sturdy[id][2]
        && g_sturdy[id][3] && g_sturdy[id][4] && g_sturdy[id][5];
}
const std::set<std::string> REDSTONE_NEVER = { "glass" };
bool isRedstoneConductor(int id) {
    if (REDSTONE_NEVER.count(g_name[id])) return false;
    return isCollisionFullBlock(id);
}
// RedStoneWireBlock.shouldConnectTo (:388).
bool wireShouldConnectTo(int id, int direction) {  // direction = -1 means "null"
    const std::string& n = g_name[id];
    if (n == "redstone_wire") return true;
    if (n == "repeater") {
        int rd = dirFromName(getProp(g_props[id], "facing"));
        return rd == direction || opposite(rd) == direction;
    }
    if (n == "observer")
        return direction >= 0 && direction == dirFromName(getProp(g_props[id], "facing"));
    return false;
}
bool wireCanSurviveOn(int id) { return isFaceSturdy(id, UP) || g_name[id] == "hopper"; }  // :267
std::string wireConnectingSide(const Level& level, int direction, bool canConnectUp) {  // :240-258
    int rdx = DX[direction], rdy = DY[direction], rdz = DZ[direction];
    int relId = level.at(rdx, rdy, rdz);
    if (canConnectUp) {
        bool isPlaceableAbove = (g_name[relId].size() >= 8
                && g_name[relId].compare(g_name[relId].size() - 8, 8, "trapdoor") == 0)
            || wireCanSurviveOn(relId);
        int aboveRel = level.at(rdx, rdy + 1, rdz);
        if (isPlaceableAbove && wireShouldConnectTo(aboveRel, -1)) {
            if (isFaceSturdy(relId, opposite(direction))) return "up";
            return "side";
        }
    }
    int belowRel = level.at(rdx, rdy - 1, rdz);
    return (!wireShouldConnectTo(relId, direction)
            && (isRedstoneConductor(relId) || !wireShouldConnectTo(belowRel, -1)))
        ? "none" : "side";
}
bool wireIsCross(int id) {
    const std::string& p = g_props[id];
    return sideConnected(getProp(p, "north")) && sideConnected(getProp(p, "south"))
        && sideConnected(getProp(p, "east")) && sideConnected(getProp(p, "west"));
}
bool wireIsDot(int id) {
    const std::string& p = g_props[id];
    return !sideConnected(getProp(p, "north")) && !sideConnected(getProp(p, "south"))
        && !sideConnected(getProp(p, "east")) && !sideConnected(getProp(p, "west"));
}
int wireDefault(const std::string& power) {
    auto it = g_index.find(std::string("redstone_wire") + "\x1f"
        + "east=none,north=none,power=" + power + ",south=none,west=none");
    return it == g_index.end() ? -1 : it->second;
}
int wireCross(const std::string& power) {
    auto it = g_index.find(std::string("redstone_wire") + "\x1f"
        + "east=side,north=side,power=" + power + ",south=side,west=side");
    return it == g_index.end() ? -1 : it->second;
}
int wireMissingConnections(const Level& level, int id) {  // :156-167
    bool canConnectUp = !isRedstoneConductor(level.at(0, 1, 0));
    for (int hi = 0; hi < 4; ++hi) {
        int direction = HORIZONTAL[hi];
        std::string key = horizProp(direction);
        if (!sideConnected(getProp(g_props[id], key))) {
            std::string sc = wireConnectingSide(level, direction, canConnectUp);
            int ns = setProp(id, key, sc); if (ns >= 0) id = ns;
        }
    }
    return id;
}
int wireConnectionState(const Level& level, int id) {  // :124-154
    bool wasDot = wireIsDot(id);
    std::string power = getProp(g_props[id], "power");
    int base = wireDefault(power); if (base < 0) base = id;
    id = wireMissingConnections(level, base);
    if (wasDot && wireIsDot(id)) return id;
    bool north = sideConnected(getProp(g_props[id], "north"));
    bool south = sideConnected(getProp(g_props[id], "south"));
    bool east = sideConnected(getProp(g_props[id], "east"));
    bool west = sideConnected(getProp(g_props[id], "west"));
    bool nsEmpty = !north && !south, ewEmpty = !east && !west;
    if (!west && nsEmpty) { int ns = setProp(id, "west", "side"); if (ns >= 0) id = ns; }
    if (!east && nsEmpty) { int ns = setProp(id, "east", "side"); if (ns >= 0) id = ns; }
    if (!north && ewEmpty) { int ns = setProp(id, "north", "side"); if (ns >= 0) id = ns; }
    if (!south && ewEmpty) { int ns = setProp(id, "south", "side"); if (ns >= 0) id = ns; }
    return id;
}
bool instrWorksAbove(const std::string& instr) {  // NoteBlockInstrument.worksAboveNoteBlock (:64)
    static const std::set<std::string> W = { "zombie", "skeleton", "creeper", "dragon",
        "wither_skeleton", "piglin", "custom_head" };
    return W.count(instr) != 0;
}
std::string instrumentOf(int id) {  // BlockState.instrument() (Blocks.java; default HARP)
    const std::string& n = g_name[id];
    if (n == "stone" || n == "cobblestone_wall" || n == "nether_brick_fence") return "basedrum";
    if (n == "glass" || n == "glass_pane") return "hat";
    if (n == "oak_fence" || n == "oak_stairs" || n == "oak_slab" || n == "chest" || n == "note_block")
        return "bass";
    return "harp";
}
int noteSetInstrument(const Level& level, int id) {  // NoteBlock.setInstrument (:54-63)
    std::string ia = instrumentOf(level.at(0, 1, 0));
    if (instrWorksAbove(ia)) { int ns = setProp(id, "instrument", ia); return ns < 0 ? id : ns; }
    std::string ib = instrumentOf(level.at(0, -1, 0));
    std::string newb = instrWorksAbove(ib) ? "harp" : ib;
    int ns = setProp(id, "instrument", newb); return ns < 0 ? id : ns;
}
bool vineCanSupportAtFace(const Level& level, int direction) {  // VineBlock :99-116
    if (direction == DOWN) return false;
    int rdx = DX[direction], rdy = DY[direction], rdz = DZ[direction];
    if (canAttachTo(level.at(rdx, rdy, rdz), direction)) return true;
    if (axisOf(direction) == 1) return false;
    int above = level.at(0, 1, 0);
    return g_name[above] == "vine" && getProp(g_props[above], horizProp(direction)) == "true";
}
bool vineHasFaces(int id) {
    return getProp(g_props[id], "up") == "true" || getProp(g_props[id], "north") == "true"
        || getProp(g_props[id], "east") == "true" || getProp(g_props[id], "south") == "true"
        || getProp(g_props[id], "west") == "true";
}
int vineUpdatedState(const Level& level, int id) {  // VineBlock.getUpdatedState (:122-147)
    if (getProp(g_props[id], "up") == "true") {
        bool v = canAttachTo(level.at(0, 1, 0), DOWN);
        int ns = setProp(id, "up", v ? "true" : "false"); if (ns >= 0) id = ns;
    }
    int aboveState = -1;
    for (int hi = 0; hi < 4; ++hi) {
        int direction = HORIZONTAL[hi];
        std::string key = horizProp(direction);
        if (getProp(g_props[id], key) == "true") {
            bool canSup = vineCanSupportAtFace(level, direction);
            if (!canSup) {
                if (aboveState < 0) aboveState = level.at(0, 1, 0);
                canSup = g_name[aboveState] == "vine" && getProp(g_props[aboveState], key) == "true";
            }
            int ns = setProp(id, key, canSup ? "true" : "false"); if (ns >= 0) id = ns;
        }
    }
    return id;
}
bool tripwireShouldConnectTo(int nbrId, int direction) {  // TripWireBlock.shouldConnectTo (:196)
    const std::string& n = g_name[nbrId];
    if (n == "tripwire_hook")
        return dirFromName(getProp(g_props[nbrId], "facing")) == opposite(direction);
    return n == "tripwire";
}
bool mossCanSupportAtFace(const Level& level, int direction) {  // MossyCarpetBlock :123-125
    if (direction == UP) return false;
    return canAttachTo(level.rel(direction), direction);
}
bool mossHasFaces(int id) {  // :109-121
    if (getProp(g_props[id], "bottom") == "true") return true;
    for (int hi = 0; hi < 4; ++hi)
        if (getProp(g_props[id], horizProp(HORIZONTAL[hi])) != "none") return true;
    return false;
}
bool mossCanSurvive(const Level& level, int id) {  // :104-107
    int below = level.at(0, -1, 0);
    if (getProp(g_props[id], "bottom") == "true") return g_name[below] != "air";
    return g_name[below] == "pale_moss_carpet" && getProp(g_props[below], "bottom") == "true";
}
int mossUpdatedState(const Level& level, int id, bool createSides) {  // :127-159
    int aboveState = -1, belowState = -1;
    createSides = createSides || (getProp(g_props[id], "bottom") == "true");
    for (int hi = 0; hi < 4; ++hi) {
        int direction = HORIZONTAL[hi];
        std::string key = horizProp(direction);
        std::string side;
        if (mossCanSupportAtFace(level, direction))
            side = createSides ? "low" : getProp(g_props[id], key);
        else
            side = "none";
        if (side == "low") {
            if (aboveState < 0) aboveState = level.at(0, 1, 0);
            if (g_name[aboveState] == "pale_moss_carpet" && getProp(g_props[aboveState], key) != "none"
                && getProp(g_props[aboveState], "bottom") != "true")
                side = "tall";
            if (getProp(g_props[id], "bottom") != "true") {
                if (belowState < 0) belowState = level.at(0, -1, 0);
                if (g_name[belowState] == "pale_moss_carpet" && getProp(g_props[belowState], key) == "none")
                    side = "none";
            }
        }
        int ns = setProp(id, key, side); if (ns >= 0) id = ns;
    }
    return id;
}
bool amethystCanSurvive(const Level& level, int id) {  // AmethystClusterBlock :58-62
    int facing = dirFromName(getProp(g_props[id], "facing"));
    int adj = opposite(facing);
    return isFaceSturdy(level.rel(adj), facing);
}

// updateShape dispatch (keyed by the updateShape declaring class). Returns the new state id, or
// -2 if the family is not yet ported (-> todo). A ported family that leaves the state unchanged
// returns stateId.
const std::set<std::string> PORTED = {
    "StairBlock", "DoorBlock", "BedBlock", "ChestBlock", "CopperChestBlock", "TrapDoorBlock",
    "WallBlock", "FenceBlock", "IronBarsBlock", "FenceGateBlock",
    // agent C (attachment/survival)
    "FaceAttachedHorizontalDirectionalBlock", "MultifaceBlock", "CeilingHangingSignBlock",
    "StandingSignBlock", "BannerBlock", "WallBannerBlock", "WallSignBlock",
    // agent D (redstone/note/vine/tripwire/moss/amethyst + no-op verifies)
    "RedStoneWireBlock", "RepeaterBlock", "NoteBlock", "VineBlock", "TripWireBlock",
    "MossyCarpetBlock", "AmethystClusterBlock", "SlabBlock", "CandleBlock", "LeavesBlock",
    "ShelfBlock", "CopperGolemStatueBlock",
    // straggler wave 1
    "LightningRodBlock", "ChainBlock", "SculkSensorBlock", "BeehiveBlock", "HugeMushroomBlock",
    "CampfireBlock", "BaseCoralWallFanBlock", "CoralWallFanBlock", "BasePressurePlateBlock",
    // straggler wave 2 (no-ops)
    "ScaffoldingBlock", "LightBlock", "DriedGhastBlock", "LiquidBlock",
    "CandleCakeBlock", "BaseCoralPlantTypeBlock", "FlowerPotBlock", "LanternBlock",
    // straggler wave 3
    "CarpetBlock", "TripWireHookBlock", "WaterloggedTransparentBlock", "ComparatorBlock",
    // straggler wave 4
    "CactusBlock", "FallingBlock", "SugarCaneBlock", "DecoratedPotBlock",
    "CreakingHeartBlock", "CakeBlock", "WallTorchBlock", "RedstoneWallTorchBlock", "SnowLayerBlock",
    "ConcretePowderBlock",
    // straggler wave 5 (#supports_vegetation plants)
    "VegetationBlock", "DoublePlantBlock",
    // straggler wave 6
    "SeaPickleBlock", "AttachedStemBlock", "PistonHeadBlock",
    // straggler wave 7
    "CocoaBlock", "CoralPlantBlock", "CoralFanBlock", "WallHangingSignBlock",
    // straggler wave 8
    "BambooStalkBlock", "BigDripleafBlock", "MangrovePropaguleBlock", "SmallDripleafBlock",
    "BaseRailBlock",
    // straggler wave 9
    "EnderChestBlock", "SculkShriekerBlock", "ObserverBlock", "BrushableBlock", "FarmlandBlock",
    "CoralBlock", "ChorusFlowerBlock", "BigDripleafStemBlock", "LadderBlock", "BaseTorchBlock", "SnowyBlock",
    // straggler wave 10
    "BarrierBlock", "BubbleColumnBlock", "ConduitBlock", "HeavyCoreBlock", "MangroveRootsBlock",
    "DirtPathBlock", "HangingMossBlock"
};

int updateShapeOne(const std::string& fam, int stateId, int dir, int neighbourId, const Level& level) {
    if (fam == "StairBlock") {
        // StairBlock.updateShape :111-128 — horizontal dir recomputes SHAPE; vertical unchanged.
        if (isHorizontal(dir)) {
            int ns = setProp(stateId, "shape", getStairsShape(stateId, level));
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    if (fam == "DoorBlock") {
        // DoorBlock.updateShape :87-108. half = HALF (lower/upper).
        bool lower = getProp(g_props[stateId], "half") == "lower";
        bool dirIsY = (dir == DOWN || dir == UP);
        // if (dir.getAxis() != Y || (half==LOWER) != (dir==UP))
        if (!dirIsY || (lower != (dir == UP))) {
            // support-loss check only when LOWER && dir==DOWN && !canSurvive; else no-op (super).
            if (lower && dir == DOWN && !isFaceSturdy(level.rel(DOWN), UP))
                return 0;  // Blocks.AIR
            return stateId;  // super.updateShape -> unchanged
        }
        // toward the other half: neighbour is a door of the OTHER half -> copy our half onto it; else AIR.
        if (isDoorName(g_name[neighbourId]) && getProp(g_props[neighbourId], "half") != getProp(g_props[stateId], "half")) {
            int ns = setProp(neighbourId, "half", getProp(g_props[stateId], "half"));
            return ns < 0 ? 0 : ns;
        }
        return 0;  // Blocks.AIR
    }
    if (fam == "BedBlock") {
        // BedBlock.updateShape :154-176. getNeighbourDirection(part,facing): FOOT->facing, HEAD->opposite.
        std::string part = getProp(g_props[stateId], "part");
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        int neighbourDir = (part == "foot") ? facing : opposite(facing);
        if (dir != neighbourDir) return stateId;  // super.updateShape -> unchanged
        // neighbourState.is(this) (same bed block) && part differs -> copy neighbour's OCCUPIED; else AIR.
        if (g_name[neighbourId] == g_name[stateId] && getProp(g_props[neighbourId], "part") != part) {
            int ns = setProp(stateId, "occupied", getProp(g_props[neighbourId], "occupied"));
            return ns < 0 ? 0 : ns;
        }
        return 0;  // Blocks.AIR
    }
    if (fam == "ChestBlock") {
        // ChestBlock.updateShape :157-185. (WATERLOGGED only schedules a water tick: no state change.)
        // chestCanConnectTo(neighbour)==neighbour.is(this) (same block) && dir horizontal:
        if (g_name[neighbourId] == g_name[stateId] && isHorizontal(dir)) {
            std::string nType = getProp(g_props[neighbourId], "type");
            if (getProp(g_props[stateId], "type") == "single" && nType != "single"
                && getProp(g_props[stateId], "facing") == getProp(g_props[neighbourId], "facing")
                && chestConnectedDirection(neighbourId) == opposite(dir)) {
                int ns = setProp(stateId, "type", chestTypeOpposite(nType));
                return ns < 0 ? stateId : ns;
            }
        } else if (chestConnectedDirection(stateId) == dir) {
            int ns = setProp(stateId, "type", "single");
            return ns < 0 ? stateId : ns;
        }
        return stateId;  // super.updateShape -> unchanged
    }
    if (fam == "CopperChestBlock") {
        // CopperChestBlock.updateShape :98-118 — runs ChestBlock logic first, then a copper re-color.
        // chestCanConnectTo (CopperChestBlock :66-69) = neighbour.is(BlockTags.COPPER_CHESTS) &&
        // neighbour.hasProperty(ChestBlock.TYPE). The GT oracle (BlockUpdateShapeParity.java) runs only
        // Bootstrap.bootStrap(), which registers blocks but NEVER loads the data/ datapack block tags, so
        // BlockTags.COPPER_CHESTS is empty there -> is(#copper_chests) is FALSE for every block. We match
        // that oracle: chestCanConnectTo is always false, so a copper chest never connects (only the
        // disconnect-to-SINGLE else-if fires) and the re-color path is never reached. Verified against GT:
        // all 768 copper-chest scenarios output type=single, never left/right. (isCopperChestName is the
        // tag membership; AND with false models the empty oracle tag.)
        bool nbrCanConnect = isCopperChestName(g_name[neighbourId]);  // COPPER_CHESTS tag (GT now tag-bound)
        int blockState = stateId;
        // --- super (ChestBlock) part: same as ChestBlock above but with the copper connect predicate. ---
        if (nbrCanConnect && isHorizontal(dir)) {
            std::string nType = getProp(g_props[neighbourId], "type");
            if (getProp(g_props[blockState], "type") == "single" && nType != "single"
                && getProp(g_props[blockState], "facing") == getProp(g_props[neighbourId], "facing")
                && chestConnectedDirection(neighbourId) == opposite(dir)) {
                int ns = setProp(blockState, "type", chestTypeOpposite(nType));
                if (ns >= 0) blockState = ns;
            }
        } else if (chestConnectedDirection(blockState) == dir) {
            int ns = setProp(blockState, "type", "single");
            if (ns >= 0) blockState = ns;
        }
        // --- copper re-color: if connectable && TYPE!=SINGLE && connectedDir==dir -> match neighbour block. ---
        if (nbrCanConnect) {
            if (getProp(g_props[blockState], "type") != "single" && chestConnectedDirection(blockState) == dir) {
                int ns = withChestBlockOf(g_name[neighbourId], blockState);
                return ns < 0 ? blockState : ns;
            }
        }
        return blockState;
    }
    if (fam == "TrapDoorBlock") {
        // TrapDoorBlock.updateShape :178-194 — WATERLOGGED only schedules a water tick; calls
        // super.updateShape (no-op). State is never changed.
        return stateId;
    }
    if (fam == "FenceBlock") {
        // FenceBlock.updateShape :98-121 — horizontal dir recomputes that side's connection via
        // connectsTo(neighbour, neighbour.isFaceSturdy(opp), opp). Vertical -> super (no-op).
        if (!isHorizontal(dir)) return stateId;
        int opp = opposite(dir);
        bool conn = fenceConnectsTo(neighbourId, stateId, isFaceSturdy(neighbourId, opp), opp);
        const char* key = (dir == NORTH) ? "north" : (dir == SOUTH) ? "south"
                        : (dir == WEST) ? "west" : "east";
        int ns = setProp(stateId, key, conn ? "true" : "false");
        return ns < 0 ? stateId : ns;
    }
    if (fam == "IronBarsBlock") {
        // IronBarsBlock.updateShape :57-78 — horizontal dir recomputes that side via
        // attachsTo(neighbour, neighbour.isFaceSturdy(opp)). Vertical -> super (no-op). Panes share class.
        if (!isHorizontal(dir)) return stateId;
        int opp = opposite(dir);
        bool conn = barsAttachsTo(neighbourId, isFaceSturdy(neighbourId, opp));
        const char* key = (dir == NORTH) ? "north" : (dir == SOUTH) ? "south"
                        : (dir == WEST) ? "west" : "east";
        int ns = setProp(stateId, key, conn ? "true" : "false");
        return ns < 0 ? stateId : ns;
    }
    if (fam == "FenceGateBlock") {
        // FenceGateBlock.updateShape :78-96 — only when FACING.getClockWise().getAxis()==dir.getAxis()
        // does it recompute IN_WALL = isWall(neighbour) || isWall(block at pos.relative(opposite(dir)));
        // otherwise super.updateShape (no-op). isWall = state.is(#walls).
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        auto axisOf = [](int d){ return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); };
        if (axisOf(clockWise(facing)) != axisOf(dir)) return stateId;  // super -> unchanged
        int behindId = level.rel(opposite(dir));
        bool inWall = inTag(g_tags.walls, neighbourId) || inTag(g_tags.walls, behindId);
        int ns = setProp(stateId, "in_wall", inWall ? "true" : "false");
        return ns < 0 ? stateId : ns;
    }
    if (fam == "WallBlock") {
        // WallBlock.updateShape :134-156. WATERLOGGED only schedules a tick. DOWN -> super (no-op).
        // UP -> topUpdate; horizontal side -> sideUpdate. Both recompute the 4 sides + UP from the
        // above block's collision face shape.
        if (dir == DOWN) return stateId;  // super -> unchanged
        if (dir == UP) {
            // topUpdate :166-172 — keep current side connections; recompute against the new top.
            bool n = wallIsConnected(stateId, "north"), e = wallIsConnected(stateId, "east");
            bool s = wallIsConnected(stateId, "south"), w = wallIsConnected(stateId, "west");
            return wallApply(stateId, n, e, s, w, neighbourId);  // topNeighbour = the UP neighbour
        }
        // sideUpdate :174-193 — recompute the touched side from the neighbour, keep the other three;
        // the above block is whatever sits at pos.above() (the UP cell).
        int opp = opposite(dir);
        bool n = (dir == NORTH) ? wallConnectsTo(neighbourId, isFaceSturdy(neighbourId, opp), opp)
                                : wallIsConnected(stateId, "north");
        bool e = (dir == EAST)  ? wallConnectsTo(neighbourId, isFaceSturdy(neighbourId, opp), opp)
                                : wallIsConnected(stateId, "east");
        bool s = (dir == SOUTH) ? wallConnectsTo(neighbourId, isFaceSturdy(neighbourId, opp), opp)
                                : wallIsConnected(stateId, "south");
        bool w = (dir == WEST)  ? wallConnectsTo(neighbourId, isFaceSturdy(neighbourId, opp), opp)
                                : wallIsConnected(stateId, "west");
        int aboveId = level.rel(UP);
        return wallApply(stateId, n, e, s, w, aboveId);
    }
    // ── agent C (attachment / survival blocks; AIR on support loss). ──
    if (fam == "FaceAttachedHorizontalDirectionalBlock") {
        // :57-71 — AIR when the attachment neighbour can no longer support it. GrindstoneBlock
        // (FAM==this, no updateShape override) overrides canSurvive->true (GrindstoneBlock.java:69).
        if (g_name[stateId] == "grindstone") return stateId;
        int cd = faceConnectedDir(stateId);
        if (opposite(cd) == dir) {
            if (!isFaceSturdy(level.rel(dir), opposite(dir))) return 0;
        }
        return stateId;
    }
    if (fam == "MultifaceBlock") {
        // MultifaceBlock.updateShape :123-145 — drop the face toward dir if its neighbour can no
        // longer support it (canAttachTo = support||collision face full); AIR if none remain.
        if (!mfHasAnyFace(stateId)) return 0;
        if (mfHasFace(stateId, dir) && !multifaceCanAttachTo(level.rel(dir), dir)) {
            int ns = setProp(stateId, faceProp(dir), "false");
            if (ns < 0) return stateId;
            return mfHasAnyFace(ns) ? ns : 0;
        }
        return stateId;
    }
    if (fam == "CeilingHangingSignBlock") {
        // :137-151 — AIR if support above lost; canSurvive :91-93 = above.isFaceSturdy(DOWN, CENTER).
        if (dir == UP && !isCenterSupport(level.rel(UP), DOWN)) return 0;
        return stateId;
    }
    if (fam == "StandingSignBlock") {  // :50-64 — canSurvive :37-40 = below.isSolid().
        if (dir == DOWN && !isSolidState(level.rel(DOWN))) return 0;
        return stateId;
    }
    if (fam == "BannerBlock") {  // :56-70 — canSurvive :41-44 = below.isSolid().
        if (dir == DOWN && !isSolidState(level.rel(DOWN))) return 0;
        return stateId;
    }
    if (fam == "WallBannerBlock") {  // :44-58 — canSurvive = relative(FACING.opposite()).isSolid().
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (dir == opposite(facing) && !isSolidState(level.rel(opposite(facing)))) return 0;
        return stateId;
    }
    if (fam == "WallSignBlock") {  // :74-88 — canSurvive = relative(FACING.opposite()).isSolid().
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (opposite(dir) == facing && !isSolidState(level.rel(opposite(facing)))) return 0;
        return stateId;
    }

    // ── agent D (redstone / note / vine / tripwire / moss / amethyst + no-op verifies). ──
    if (fam == "RedStoneWireBlock") {
        // RedStoneWireBlock.updateShape :169-194.
        if (dir == DOWN) return wireCanSurviveOn(level.rel(DOWN)) ? stateId : 0;
        if (dir == UP) return wireConnectionState(level, stateId);
        bool canConnectUp = !isRedstoneConductor(level.at(0, 1, 0));
        std::string sc = wireConnectingSide(level, dir, canConnectUp);
        std::string key = horizProp(dir);
        if (sideConnected(sc) == sideConnected(getProp(g_props[stateId], key)) && !wireIsCross(stateId)) {
            int ns = setProp(stateId, key, sc); return ns < 0 ? stateId : ns;
        }
        std::string power = getProp(g_props[stateId], "power");
        int base = wireCross(power); if (base < 0) base = stateId;
        int b2 = setProp(base, key, sc); if (b2 >= 0) base = b2;
        return wireConnectionState(level, base);
    }
    if (fam == "RepeaterBlock") {
        // RepeaterBlock.updateShape :62-80 — DOWN survival (DiodeBlock.canSurviveOn = isFaceSturdy(UP,
        // RIGID)); perpendicular -> LOCKED=false.
        if (dir == DOWN && !isRigidSupport(level.rel(DOWN), UP)) return 0;
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (axisOf(dir) != axisOf(facing)) {
            int ns = setProp(stateId, "locked", "false"); return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    if (fam == "NoteBlock") {  // :70-85 — vertical neighbour recomputes INSTRUMENT.
        if (axisOf(dir) == 1) return noteSetInstrument(level, stateId);
        return stateId;
    }
    if (fam == "VineBlock") {  // :149-166 — DOWN unchanged; else recompute faces, AIR if none.
        if (dir == DOWN) return stateId;
        int bs = vineUpdatedState(level, stateId);
        return vineHasFaces(bs) ? bs : 0;
    }
    if (fam == "TripWireBlock") {  // :84-98 — horizontal neighbour sets that side.
        if (isHorizontal(dir)) {
            int ns = setProp(stateId, horizProp(dir),
                tripwireShouldConnectTo(level.rel(dir), dir) ? "true" : "false");
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    if (fam == "MossyCarpetBlock") {  // :210-227 — AIR if !canSurvive or no faces; else updated state.
        if (!mossCanSurvive(level, stateId)) return 0;
        int bs = mossUpdatedState(level, stateId, false);
        return mossHasFaces(bs) ? bs : 0;
    }
    if (fam == "AmethystClusterBlock") {  // :64-82 — AIR when support face lost.
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (dir == opposite(facing) && !amethystCanSurvive(level, stateId)) return 0;
        return stateId;
    }
    // No-op verifies — updateShape only (conditionally) schedules a tick, returns state unchanged:
    // SlabBlock :116-131, CandleBlock :116-132, LeavesBlock :88-109, ShelfBlock :295-311,
    // CopperGolemStatueBlock :167-183.
    if (fam == "SlabBlock" || fam == "CandleBlock" || fam == "LeavesBlock"
        || fam == "ShelfBlock" || fam == "CopperGolemStatueBlock")
        return stateId;

    // ── straggler wave 1 (no-op + simple sturdy/center canSurvive, transcribed 1:1). ──
    // Pure no-ops (waterlogged tick / side effect, super returns state unchanged):
    // LightningRodBlock, ChainBlock, SculkSensorBlock, BeehiveBlock (fire-emergency side effect only).
    if (fam == "LightningRodBlock" || fam == "ChainBlock" || fam == "SculkSensorBlock"
        || fam == "BeehiveBlock" || fam == "ScaffoldingBlock" || fam == "LightBlock"
        || fam == "DriedGhastBlock" || fam == "LiquidBlock"
        // wave 4 no-ops (schedule a self/water tick, super returns state unchanged):
        || fam == "CactusBlock" || fam == "FallingBlock" || fam == "SugarCaneBlock"
        || fam == "DecoratedPotBlock" || fam == "CreakingHeartBlock"
        // BaseRailBlock.updateShape — waterlogged tick + super -> no-op (powered/detector/activator rails;
        // the rail SHAPE recompute lives in RailBlock, a separate FAM).
        || fam == "BaseRailBlock"
        // wave 9 no-ops: EnderChest/SculkShrieker/Brushable/BigDripleafStem (water/self tick),
        // ObserverBlock (startSignal side effect), FarmlandBlock/ChorusFlowerBlock (canSurvive->tick),
        // CoralBlock (scanForWater->tick) — all return super/state unchanged.
        || fam == "EnderChestBlock" || fam == "SculkShriekerBlock" || fam == "ObserverBlock"
        || fam == "BrushableBlock" || fam == "FarmlandBlock" || fam == "CoralBlock"
        || fam == "ChorusFlowerBlock" || fam == "BigDripleafStemBlock"
        // wave 10 no-ops (water/self tick, super/state unchanged):
        || fam == "BarrierBlock" || fam == "BubbleColumnBlock" || fam == "ConduitBlock"
        || fam == "HeavyCoreBlock" || fam == "MangroveRootsBlock" || fam == "DirtPathBlock")
        return stateId;
    // HugeMushroomBlock.updateShape — neighbour.is(this) ? clear PROPERTY_BY_DIRECTION[dir] : super.
    if (fam == "HugeMushroomBlock") {
        if (g_name[neighbourId] == g_name[stateId]) {
            int ns = setProp(stateId, faceProp(dir), "false");
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    // CampfireBlock.updateShape — DOWN -> SIGNAL_FIRE = isSmokeSource(below) (== is(HAY_BLOCK)); else super.
    if (fam == "CampfireBlock") {
        if (dir == DOWN) {
            int ns = setProp(stateId, "signal_fire", g_name[neighbourId] == "hay_block" ? "true" : "false");
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    // BaseCoralWallFanBlock / CoralWallFanBlock.updateShape — opposite(dir)==FACING && !canSurvive -> AIR.
    // canSurvive = isFaceSturdy(relative(FACING.opposite()), FACING).
    if (fam == "BaseCoralWallFanBlock" || fam == "CoralWallFanBlock") {
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (opposite(dir) == facing && !isFaceSturdy(level.rel(opposite(facing)), facing)) return 0;
        return stateId;
    }
    // BasePressurePlateBlock.updateShape — DOWN && !canSurvive -> AIR. canSurvive (:?) =
    // canSupportRigidBlock(below) || canSupportCenter(below, UP); RIGID==FULL for the probe pool.
    if (fam == "BasePressurePlateBlock") {
        if (dir == DOWN) {
            int below = level.rel(DOWN);  // canSupportRigidBlock(below) || canSupportCenter(below, UP)
            if (!(isRigidSupport(below, UP) || isCenterSupport(below, UP))) return 0;
        }
        return stateId;
    }

    // ── straggler wave 2 (no-op + simple canSurvive). ──
    // CandleCakeBlock.updateShape — DOWN && !canSurvive(below.isSolid()) -> AIR.
    if (fam == "CandleCakeBlock") {
        if (dir == DOWN && !isSolidState(level.rel(DOWN))) return 0;
        return stateId;
    }
    // BaseCoralPlantTypeBlock.updateShape — DOWN && !canSurvive(below.isFaceSturdy(UP)) -> AIR.
    if (fam == "BaseCoralPlantTypeBlock") {
        if (dir == DOWN && !isFaceSturdy(level.rel(DOWN), UP)) return 0;
        return stateId;
    }
    // FlowerPotBlock.updateShape — DOWN && !canSurvive -> AIR, but FlowerPotBlock does NOT override
    // canSurvive (default == true), so the AIR branch never fires -> no-op.
    if (fam == "FlowerPotBlock") return stateId;
    // LanternBlock.updateShape — getConnectedDirection().getOpposite()==dir && !canSurvive -> AIR.
    // getConnectedDirection :73-75 = HANGING ? DOWN : UP; canSurvive = canSupportCenter(rel(d), cd).
    // (UNSTABLE_BOTTOM_CENTER affects only cd==DOWN; no probe-pool block is in that tag.)
    if (fam == "LanternBlock") {
        int cd = getProp(g_props[stateId], "hanging") == "true" ? DOWN : UP;
        int d = opposite(cd);
        if (dir == d && !isCenterSupport(level.rel(d), cd)) return 0;
        return stateId;
    }
    // CarpetBlock.updateShape — !canSurvive(below not air) -> AIR (any direction).
    if (fam == "CarpetBlock")
        return g_name[level.rel(DOWN)] == "air" ? 0 : stateId;
    // TripWireHookBlock.updateShape — opposite(dir)==FACING && !canSurvive(behind isFaceSturdy(FACING)) -> AIR.
    if (fam == "TripWireHookBlock") {
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (opposite(dir) == facing && !isFaceSturdy(level.rel(opposite(facing)), facing)) return 0;
        return stateId;
    }
    // WaterloggedTransparentBlock.updateShape — waterlogged tick only -> no-op.
    if (fam == "WaterloggedTransparentBlock") return stateId;
    // ComparatorBlock.updateShape — DOWN && !canSurviveOn(below) -> AIR; canSurviveOn (DiodeBlock) =
    // below.isFaceSturdy(UP, SupportType.RIGID).
    if (fam == "ComparatorBlock") {
        if (dir == DOWN && !isRigidSupport(level.rel(DOWN), UP)) return 0;
        return stateId;
    }

    // ── straggler wave 4 (canSurvive + concrete solidify). ──
    // CakeBlock.updateShape — DOWN && !canSurvive(below.isSolid()) -> AIR.
    if (fam == "CakeBlock") {
        if (dir == DOWN && !isSolidState(level.rel(DOWN))) return 0;
        return stateId;
    }
    // WallTorchBlock / RedstoneWallTorchBlock.updateShape — opposite(dir)==FACING && !canSurvive -> AIR.
    // canSurvive (WallTorchBlock.java:56-59) = relative(FACING.opposite()).isFaceSturdy(FACING).
    if (fam == "WallTorchBlock" || fam == "RedstoneWallTorchBlock") {
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (opposite(dir) == facing && !isFaceSturdy(level.rel(opposite(facing)), facing)) return 0;
        return stateId;
    }
    // SnowLayerBlock.updateShape — !canSurvive -> AIR. canSurvive (SnowLayerBlock :?) ignoring the
    // CANNOT_SUPPORT/SUPPORT_OVERRIDE tags (no probe-pool block is in either) =
    // isFaceFull(below.getCollisionShape, UP) || (below.is(this) && below.LAYERS==8).
    if (fam == "SnowLayerBlock") {
        int below = level.rel(DOWN);
        bool surv = faceFullCol(below, UP)
            || (g_name[below] == "snow" && getProp(g_props[below], "layers") == "8");
        return surv ? stateId : 0;
    }
    // ConcretePowderBlock.updateShape — touchesLiquid(level,pos) -> concrete; else super.
    // touchesLiquid (:?): exists dir where (dir!=DOWN || canSolidify(self)) && canSolidify(neighbour)
    // && !neighbour.isFaceSturdy(opposite(dir)). canSolidify (:73) = fluid is water.
    if (fam == "ConcretePowderBlock") {
        auto canSolidify = [&](int id){ return g_name[id] == "water" || getProp(g_props[id], "waterlogged") == "true"; };
        bool touches = false;
        for (int d = 0; d < 6 && !touches; ++d) {
            if (d == DOWN && !canSolidify(stateId)) continue;
            int nb = level.rel(d);
            if (canSolidify(nb) && !isFaceSturdy(nb, opposite(d))) touches = true;
        }
        if (touches) {
            std::string n = g_name[stateId];
            if (endsWith(n, "_powder")) n = n.substr(0, n.size() - 7);  // white_concrete_powder -> white_concrete
            auto it = g_index.find(n + "\x1f");
            if (it != g_index.end()) return it->second;
        }
        return stateId;
    }

    // ── straggler wave 5 (plants on #supports_vegetation). ──
    // VegetationBlock.updateShape — !canSurvive -> AIR (any direction). canSurvive (:?) =
    // mayPlaceOn(below) = below.is(#supports_vegetation) for blocks using the DEFAULT mayPlaceOn
    // (most grasses/flowers). Subclasses that override mayPlaceOn (nether plants, etc.) are excluded.
    if (fam == "VegetationBlock") {
        int below = level.rel(DOWN);
        // LeafLitterBlock overrides canSurvive (:54-57) = below.isFaceSturdy(UP); default mayPlaceOn
        // (VegetationBlock :?) = below.is(#supports_vegetation).
        bool surv = (g_name[stateId] == "leaf_litter")
            ? isFaceSturdy(below, UP) : inTag(g_tags.supportsVegetation, below);
        return surv ? stateId : 0;
    }
    // DoublePlantBlock.updateShape :70-? — other-half sync. canSurvive: LOWER = mayPlaceOn(below) =
    // below.is(#supports_vegetation); UPPER = below.is(this) && below.HALF==LOWER.
    if (fam == "DoublePlantBlock") {
        std::string half = getProp(g_props[stateId], "half");
        bool isLower = (half == "lower");
        bool axisY = (dir == DOWN || dir == UP);
        bool nbrIsThis = (g_name[neighbourId] == g_name[stateId]);
        bool c1 = !axisY;
        bool c2 = (isLower) != (dir == UP);
        bool c3 = nbrIsThis && (getProp(g_props[neighbourId], "half") != half);
        if (c1 || c2 || c3) {
            if (isLower && dir == DOWN && !inTag(g_tags.supportsVegetation, level.rel(DOWN))) return 0;
            return stateId;  // super (no-op)
        }
        return 0;  // toward the other half but it's missing/mismatched -> AIR
    }
    // SeaPickleBlock.updateShape — !canSurvive -> AIR. canSurvive = mayPlaceOn(below) =
    // !below.getCollisionShape.getFaceShape(UP).isEmpty() || below.isFaceSturdy(UP).
    if (fam == "SeaPickleBlock") {
        int below = level.rel(DOWN);
        bool surv = !faceRects(g_collBoxes[below], UP).empty() || isFaceSturdy(below, UP);
        return surv ? stateId : 0;
    }
    // PistonHeadBlock.updateShape — opposite(dir)==FACING && !canSurvive -> AIR. canSurvive =
    // isFittingBase(base) || (base.is(MOVING_PISTON) && base.FACING==FACING). isFittingBase = base is
    // the matching piston (TYPE sticky -> sticky_piston) && EXTENDED && FACING==head.FACING.
    if (fam == "PistonHeadBlock") {
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (opposite(dir) == facing) {
            int base = level.rel(opposite(facing));
            std::string bn = g_name[base];
            int bf = dirFromName(getProp(g_props[base], "facing"));
            std::string baseBlock = (getProp(g_props[stateId], "type") == "sticky") ? "sticky_piston" : "piston";
            bool fitting = (bn == baseBlock) && getProp(g_props[base], "extended") == "true" && bf == facing;
            bool moving = (bn == "moving_piston") && bf == facing;
            if (!(fitting || moving)) return 0;
        }
        return stateId;
    }
    // AttachedStemBlock.updateShape — when the FACING neighbour is no longer the fruit, revert to the
    // stem block at AGE 7. fruit/stem by name (attached_pumpkin_stem -> pumpkin / pumpkin_stem).
    if (fam == "AttachedStemBlock") {
        bool isPumpkin = (g_name[stateId] == "attached_pumpkin_stem");
        std::string fruit = isPumpkin ? "pumpkin" : "melon";
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (g_name[neighbourId] != fruit && dir == facing) {
            auto it = g_index.find(std::string(isPumpkin ? "pumpkin_stem" : "melon_stem") + "\x1f" + "age=7");
            if (it != g_index.end()) return it->second;
        }
        return stateId;
    }
    // CocoaBlock.updateShape — dir==FACING && !canSurvive -> AIR. canSurvive = relative(FACING).is(#supports_cocoa).
    if (fam == "CocoaBlock") {
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (dir == facing && !inTag(g_tags.supportsCocoa, level.rel(facing))) return 0;
        return stateId;
    }
    // CoralPlantBlock / CoralFanBlock.updateShape — DOWN && !canSurvive -> AIR; canSurvive
    // (BaseCoralPlantTypeBlock) = below.isFaceSturdy(UP).
    if (fam == "CoralPlantBlock" || fam == "CoralFanBlock") {
        if (dir == DOWN && !isFaceSturdy(level.rel(DOWN), UP)) return 0;
        return stateId;
    }
    // HangingMossBlock.updateShape — TIP = (below is not this moss); canStayAtPosition only schedules.
    if (fam == "HangingMossBlock") {
        int ns = setProp(stateId, "tip", g_name[level.rel(DOWN)] != g_name[stateId] ? "true" : "false");
        return ns < 0 ? stateId : ns;
    }
    // LadderBlock.updateShape — opposite(dir)==FACING && !canSurvive -> AIR. canSurvive = canAttachTo(
    // relative(FACING.opposite())) = behind.isFaceSturdy(FACING).
    if (fam == "LadderBlock") {
        int facing = dirFromName(getProp(g_props[stateId], "facing"));
        if (opposite(dir) == facing && !isFaceSturdy(level.rel(opposite(facing)), facing)) return 0;
        return stateId;
    }
    // BaseTorchBlock.updateShape — DOWN && !canSurvive -> AIR. canSurvive = canSupportCenter(below, UP).
    if (fam == "BaseTorchBlock") {
        if (dir == DOWN && !isCenterSupport(level.rel(DOWN), UP)) return 0;
        return stateId;
    }
    // SnowyBlock.updateShape — UP -> SNOWY = isSnowySetting(above) = above.is(#snow); else super.
    if (fam == "SnowyBlock") {
        if (dir == UP) {
            int ns = setProp(stateId, "snowy", inTag(g_tags.snow, level.rel(UP)) ? "true" : "false");
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    // MangrovePropaguleBlock.updateShape — UP && !canSurvive -> AIR. canSurvive (:?) = HANGING ?
    // above.is(#supports_hanging_mangrove_propagule) : mayPlaceOn(below)=below.is(#supports_mangrove_propagule).
    if (fam == "MangrovePropaguleBlock") {
        if (dir == UP) {
            bool hanging = getProp(g_props[stateId], "hanging") == "true";
            bool surv = hanging ? inTag(g_tags.supportsHangingMangrove, level.rel(UP))
                                : inTag(g_tags.supportsMangrovePropagule, level.rel(DOWN));
            if (!surv) return 0;
        }
        return stateId;
    }
    // SmallDripleafBlock.updateShape — waterlogged tick + super(DoublePlantBlock.updateShape); same
    // other-half logic, but LOWER canSurvive uses mayPlaceOn = below.is(#supports_small_dripleaf).
    if (fam == "SmallDripleafBlock") {
        std::string half = getProp(g_props[stateId], "half");
        bool isLower = (half == "lower");
        bool axisY = (dir == DOWN || dir == UP);
        bool nbrIsThis = (g_name[neighbourId] == g_name[stateId]);
        bool c1 = !axisY, c2 = (isLower) != (dir == UP);
        bool c3 = nbrIsThis && (getProp(g_props[neighbourId], "half") != half);
        if (c1 || c2 || c3) {
            if (isLower && dir == DOWN && !inTag(g_tags.supportsSmallDripleaf, level.rel(DOWN))) return 0;
            return stateId;
        }
        return 0;
    }
    // BambooStalkBlock.updateShape — UP neighbour is taller bamboo -> cycle(AGE) (0<->1); canSurvive
    // only schedules a tick (no returned-state change).
    if (fam == "BambooStalkBlock") {
        if (dir == UP && g_name[neighbourId] == "bamboo"
            && std::stoi(getProp(g_props[neighbourId], "age")) > std::stoi(getProp(g_props[stateId], "age"))) {
            int ns = setProp(stateId, "age", getProp(g_props[stateId], "age") == "0" ? "1" : "0");
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    // BigDripleafBlock.updateShape — DOWN && !canSurvive -> AIR; UP neighbour is this -> BIG_DRIPLEAF_STEM
    // withPropertiesOf(state). canSurvive = below is(this)||is(stem)||is(#supports_big_dripleaf).
    if (fam == "BigDripleafBlock") {
        if (dir == DOWN) {
            int below = level.rel(DOWN);
            bool surv = g_name[below] == g_name[stateId] || g_name[below] == "big_dripleaf_stem"
                || inTag(g_tags.supportsBigDripleaf, below);
            if (!surv) return 0;
        }
        if (dir == UP && g_name[neighbourId] == g_name[stateId]) {  // -> stem, copy facing+waterlogged
            auto it = g_index.find(std::string("big_dripleaf_stem") + "\x1f" + "facing="
                + getProp(g_props[stateId], "facing") + ",waterlogged=" + getProp(g_props[stateId], "waterlogged"));
            if (it != g_index.end()) return it->second;
        }
        return stateId;
    }
    // WallHangingSignBlock.updateShape — dir.axis == FACING.clockWise().axis && !canSurvive -> AIR.
    // But WallHangingSignBlock does NOT override canSurvive (the two-sides predicate at :102-106 is
    // canPlace, used at PLACEMENT only); the inherited default canSurvive == true, so the AIR branch
    // never fires -> the updateShape is a NO-OP (waterlogged tick is a side effect).
    if (fam == "WallHangingSignBlock") return stateId;

    return -2;  // unported
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/block_update_shape.tsv";
    std::string statesPath = "mcpp/src/assets/block_states.json";
    std::string shapesPath = "mcpp/build/block_collision_shape.tsv";  // STURDY + SHAPE rows
    std::string tagsDir = "26.1.2/data/minecraft/tags/block";  // connection-family block tags
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
        else if (a == "--shapes" && i + 1 < argc) shapesPath = argv[++i];
        else if (a == "--tags" && i + 1 < argc) tagsDir = argv[++i];
    }

    {
        std::ifstream f(statesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
        nlohmann::json j; f >> j;
        auto arr = j.at("states");
        g_name.resize(arr.size()); g_props.resize(arr.size()); g_solid.assign(arr.size(), 0);
        for (auto& s : arr) {
            std::size_t id = s.at("id").get<std::size_t>();
            g_name[id] = s.at("name").get<std::string>();
            g_props[id] = s.value("props", std::string());
            g_solid[id] = s.value("is_solid", false) ? 1 : 0;  // BlockState.isSolid() (legacySolid)
        }
        for (std::size_t id = 0; id < g_name.size(); ++id)
            g_index[g_name[id] + "\x1f" + g_props[id]] = (int)id;
    }
    // STURDY rows -> certified isFaceSturdy; SHAPE rows -> certified collision AABB lists (the
    // above-block face shape WallBlock.isCovered needs). Both from block_collision_shape.tsv.
    g_sturdy.assign(g_name.size(), {0,0,0,0,0,0}); g_hasSturdy.assign(g_name.size(), 0);
    g_collBoxes.assign(g_name.size(), {}); g_hasColl.assign(g_name.size(), 0);
    g_collCache.assign(g_name.size(), nullptr); g_collBuilt.assign(g_name.size(), 0);
    g_supBoxes.assign(g_name.size(), {});
    {
        std::ifstream f(shapesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << shapesPath << " (run block_collision_shape GT)\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            auto c = splitTab(line);
            if (c.empty()) continue;
            if (c[0] == "STURDY" && c.size() >= 8) {
                std::size_t id = (std::size_t)std::stoul(c[1]);
                if (id >= g_sturdy.size()) continue;
                for (int d = 0; d < 6; ++d) g_sturdy[id][d] = std::stoi(c[2 + d]);
                g_hasSturdy[id] = 1;
            } else if (c[0] == "SHAPE" && c.size() >= 3) {
                std::size_t id = (std::size_t)std::stoul(c[1]);
                if (id >= g_collBoxes.size()) continue;
                int nb = std::stoi(c[2]);
                for (int k = 0; k < nb; ++k) {
                    std::size_t o = 3 + k * 6;
                    if (o + 5 >= c.size()) break;
                    g_collBoxes[id].push_back({ std::stod(c[o]), std::stod(c[o+1]), std::stod(c[o+2]),
                                                std::stod(c[o+3]), std::stod(c[o+4]), std::stod(c[o+5]) });
                }
                g_hasColl[id] = 1;
            } else if (c[0] == "SUP" && c.size() >= 3) {
                std::size_t id = (std::size_t)std::stoul(c[1]);
                if (id >= g_supBoxes.size()) continue;
                int nb = std::stoi(c[2]);
                for (int k = 0; k < nb; ++k) {
                    std::size_t o = 3 + k * 6;
                    if (o + 5 >= c.size()) break;
                    g_supBoxes[id].push_back({ std::stod(c[o]), std::stod(c[o+1]), std::stod(c[o+2]),
                                               std::stod(c[o+3]), std::stod(c[o+4]), std::stod(c[o+5]) });
                }
            }
        }
    }
    // ── connection-family block tags (nested-tag aware) from 26.1.2/data. ──
    {
        std::unordered_map<std::string, std::unordered_set<std::string>> cache;
        // resolve(tagName) -> set of block names (no ns). #-prefixed values recurse.
        std::function<const std::unordered_set<std::string>&(const std::string&)> resolve =
            [&](const std::string& tag) -> const std::unordered_set<std::string>& {
            auto it = cache.find(tag);
            if (it != cache.end()) return it->second;
            std::unordered_set<std::string>& out = cache[tag];  // insert empty first (cycle guard)
            std::ifstream tf(tagsDir + "/" + tag + ".json", std::ios::binary);
            if (!tf) { std::cerr << "warn: missing tag " << tag << "\n"; return out; }
            nlohmann::json tj; tf >> tj;
            for (auto& v : tj.at("values")) {
                std::string s = v.is_string() ? v.get<std::string>() : v.at("id").get<std::string>();
                if (!s.empty() && s[0] == '#') {
                    const auto& nested = resolve(stripNs(s.substr(1)));
                    out.insert(nested.begin(), nested.end());
                } else {
                    out.insert(stripNs(s));
                }
            }
            return out;
        };
        g_tags.walls = resolve("walls");
        g_tags.fences = resolve("fences");
        g_tags.woodenFences = resolve("wooden_fences");
        g_tags.bars = resolve("bars");
        g_tags.wallPostOverride = resolve("wall_post_override");
        g_tags.shulkerBoxes = resolve("shulker_boxes");
        g_tags.leaves = resolve("leaves");
        g_tags.supportsVegetation = resolve("supports_vegetation");
        g_tags.supportsCocoa = resolve("supports_cocoa");
        g_tags.wallHangingSigns = resolve("wall_hanging_signs");
        g_tags.supportsBigDripleaf = resolve("supports_big_dripleaf");
        g_tags.supportsHangingMangrove = resolve("supports_hanging_mangrove_propagule");
        g_tags.supportsMangrovePropagule = resolve("supports_mangrove_propagule");
        g_tags.supportsSmallDripleaf = resolve("supports_small_dripleaf");
        g_tags.snow = resolve("snow");
    }

    // GT: OFFSETS (fixed cell order), U scenarios, FAM (block -> updateShape declaring class).
    std::vector<std::array<int,3>> offs;
    std::map<std::string, std::string> updFam;  // block name -> updateShape declaring class
    struct Scn { int centre; std::vector<int> nbr; int out; };
    std::vector<Scn> scns;
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            auto c = splitTab(line);
            if (c.empty()) continue;
            if (c[0] == "OFFSETS") {
                int n = std::stoi(c[1]);
                for (int i = 0; i < n; ++i) {
                    std::size_t o = 2 + i * 3;
                    offs.push_back({ std::stoi(c[o]), std::stoi(c[o+1]), std::stoi(c[o+2]) });
                }
            } else if (c[0] == "U") {
                Scn s; s.centre = std::stoi(c[1]);
                int n = (int)offs.size();
                for (int i = 0; i < n; ++i) s.nbr.push_back(std::stoi(c[2 + i]));
                s.out = std::stoi(c[2 + n]);
                scns.push_back(std::move(s));
            } else if (c[0] == "FAM" && c.size() >= 3) {
                std::string key = c[1]; auto col = key.find(':'); if (col != std::string::npos) key = key.substr(col + 1);
                updFam[key] = c[2];
            }
        }
    }
    // Mirror the (ns-stripped) FAM map into the global used by famOf() for instanceof IronBarsBlock /
    // FenceGateBlock / WallBlock detection inside the connection-family ports.
    g_updFam = updFam;

    long cert = 0, mis = 0, todo = 0, skipped = 0;
    std::map<std::string, long> todoFam, misFam;
    int shown = 0;
    for (const Scn& s : scns) {
        if (s.out < 0) { ++skipped; continue; }  // GT proxy couldn't serve (outId=-1)
        const std::string& cfam = updFam.count(g_name[s.centre]) ? updFam[g_name[s.centre]] : "?";
        if (!PORTED.count(cfam)) { ++todo; todoFam[cfam]++; continue; }

        // build the neighbourhood cell array (dx+1)*9+(dy+1)*3+(dz+1).
        std::array<int,27> cells; cells.fill(0);
        for (std::size_t i = 0; i < offs.size(); ++i)
            cells[(offs[i][0]+1)*9 + (offs[i][1]+1)*3 + (offs[i][2]+1)] = s.nbr[i];
        Level level{ &cells };

        int cur = s.centre;
        for (int oi = 0; oi < 6; ++oi) {
            int dir = UPDATE_SHAPE_ORDER[oi];
            const std::string& fam = updFam.count(g_name[cur]) ? updFam[g_name[cur]] : "?";
            int r = updateShapeOne(fam, cur, dir, level.rel(dir), level);
            if (r == -2) { r = cur; }  // a (mid-loop) unported family acts as no-op
            cur = r;
        }
        if (cur == s.out) ++cert;
        else {
            ++mis; misFam[cfam]++;
            if (shown++ < 16) std::cerr << "mismatch centre=" << g_name[s.centre] << "[" << g_props[s.centre]
                << "] got=" << g_name[cur] << "[" << g_props[cur] << "] want=" << g_name[s.out]
                << "[" << g_props[s.out] << "]\n";
        }
    }

    std::cout << "BlockUpdateShape certified=" << cert << " mismatches=" << mis
              << " todo=" << todo << " skipped=" << skipped << "\n";
    std::vector<std::pair<std::string,long>> tv(todoFam.begin(), todoFam.end());
    std::sort(tv.begin(), tv.end(), [](auto&a,auto&b){ return a.second > b.second; });
    std::cout << "unported updateShape families (scenarios): ";
    for (std::size_t i = 0; i < tv.size() && i < 18; ++i) std::cout << tv[i].first << "=" << tv[i].second << " ";
    std::cout << "\n";
    if (mis > 0) {
        std::vector<std::pair<std::string,long>> mv(misFam.begin(), misFam.end());
        std::sort(mv.begin(), mv.end(), [](auto&a,auto&b){ return a.second > b.second; });
        std::cout << "mismatch families: ";
        for (std::size_t i = 0; i < mv.size() && i < 18; ++i) std::cout << mv[i].first << "=" << mv[i].second << " ";
        std::cout << "\n";
    }
    return mis > 0 ? 1 : 0;
}
