#include "ChunkMesh.h"
#include "../../assets/AssetManager.h"
#include "../../world/level/block/BlockState.h"
#include "../../world/level/block/Blocks.h"
#include "../../core/Log.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace mc::render {

// Blocks the mesher renders as an X-cross (cutout) rather than a full cube. Such
// blocks must also never occlude a neighbour's face. The real game drives this
// from block models; until those are ported this is the cutout-vegetation set —
// keep it in sync with the plant families registered in Blocks.cpp. Newer (1.20+)
// coast/forest-floor blocks (leaf_litter, bush, dry grass, …) were missing, so
// they rendered as solid cubes AND culled the dirt/grass faces around them.
static bool isCrossPlant(const std::string& name) {
    static const std::unordered_set<std::string> set = {
        "short_grass", "tall_grass", "fern", "large_fern", "dead_bush",
        "dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "oxeye_daisy",
        "cornflower", "lily_of_the_valley", "red_tulip", "orange_tulip", "white_tulip", "pink_tulip",
        "torchflower", "wither_rose", "sweet_berry_bush", "sugar_cane",
        "seagrass", "tall_seagrass", "sea_pickle", "kelp", "kelp_plant", "bamboo", "bamboo_sapling",
        "leaf_litter", "bush", "firefly_bush", "wildflowers", "pink_petals",
        "short_dry_grass", "tall_dry_grass", "cactus_flower",
        "nether_sprouts", "crimson_roots", "warped_roots",
        // small mushrooms (the huge-mushroom *_block / mushroom_stem stay cubes)
        "red_mushroom", "brown_mushroom",
        // cave / lush vegetation that clings to a surface (flat, never occluding)
        "glow_lichen", "sculk_vein", "cave_vines", "cave_vines_plant", "hanging_roots",
        "spore_blossom", "big_dripleaf", "big_dripleaf_stem", "small_dripleaf",
        "pitcher_plant", "twisting_vines", "twisting_vines_plant",
        "weeping_vines", "weeping_vines_plant", "vine",
        "oak_sapling", "spruce_sapling", "birch_sapling", "jungle_sapling",
        "acacia_sapling", "dark_oak_sapling", "cherry_sapling", "pale_oak_sapling", "mangrove_propagule",
    };
    if (set.count(name)) return true;
    if (name.find("coral") != std::string::npos) return true; // coral fans/plants (pre-existing)
    return false;
}

// Face directions: +X, -X, +Y, -Y, +Z, -Z
static constexpr int DX[6] = { 1,-1, 0, 0, 0, 0 };
static constexpr int DY[6] = { 0, 0, 1,-1, 0, 0 };
static constexpr int DZ[6] = { 0, 0, 0, 0, 1,-1 };

// Quad corners (CCW winding) for each face
static constexpr float FACE_VERTS[6][4][3] = {
    {{1,0,1},{1,1,1},{1,1,0},{1,0,0}}, // +X
    {{0,0,0},{0,1,0},{0,1,1},{0,0,1}}, // -X
    {{0,1,0},{1,1,0},{1,1,1},{0,1,1}}, // +Y
    {{0,0,1},{1,0,1},{1,0,0},{0,0,0}}, // -Y
    {{0,0,1},{0,1,1},{1,1,1},{1,0,1}}, // +Z
    {{1,0,0},{1,1,0},{0,1,0},{0,0,0}}, // -Z
};

// UV atlas corner layout (matches quad winding above)
static const float UV_CORNERS[4][2] = {
    {0,1},{0,0},{1,0},{1,1}
};

static const char* FACE_NAMES[6] = { "east", "west", "up", "down", "south", "north" };

// Per-vertex tint applied as vertex color multiplier (shader does tex * vColor)
struct TintRGB { uint8_t r, g, b; };

// Biome tint per texture. Hardcoded to plains biome defaults.
// Port of net.minecraft.client.color.block.BlockColors + biome colormap lookup.
// Plains: temperature=0.8, downfall=0.4 → grass #79C05A, foliage #59AE30
static TintRGB getTextureTint(const std::string& name) {
    if (name.empty()) return {255, 255, 255};
    // Grass-colored biome blocks
    if (name == "grass_block_top"     || name == "grass_block_side_overlay" ||
        name == "short_grass"          || name == "fern"                     ||
        name == "tall_grass_top"       || name == "tall_grass_bottom"        ||
        name == "large_fern_top"       || name == "large_fern_bottom"        ||
        name == "bush"                 || name == "firefly_bush"             ||
        name == "sugar_cane")
        return {121, 192, 90};   // #79C05A plains grass
    if (name == "water_still" || name == "water_flow")
        return {63, 118, 228};   // #3F76E4 beautiful water blue
    // leaf_litter uses dryFoliage() tint (BlockColors.java:37), same constant
    if (name == "short_dry_grass" || name == "tall_dry_grass" || name == "leaf_litter")
        return {92, 60, 50};      // DryFoliageColor.FOLIAGE_DRY_DEFAULT (#5C3C32)
    // Fixed spruce leaf tint (biome-independent per BlockColors.java)
    if (name == "spruce_leaves") return {97,  153, 97};  // #619961
    // Fixed birch leaf tint (biome-independent per BlockColors.java)
    if (name == "birch_leaves")  return {128, 167, 85};  // #80A755
    // Remaining leaves & vines use biome foliage color
    if (name.ends_with("_leaves") || name == "vine") return {89, 174, 48}; // #59AE30
    return {255, 255, 255};
}

static bool isPillarTextureBlock(const std::string& name) {
    return name.ends_with("_log") || name.ends_with("_wood") ||
           name.ends_with("_stem") || name.ends_with("_hyphae") ||
           name == "bamboo_block";
}

static std::string textureForStateFace(const mc::BlockState& state, int face) {
    const mc::Block* block = state.block;
    if (!block) {
        return "";
    }

    if (isPillarTextureBlock(block->name)) {
        std::string axis = state.getProperty("axis");
        if (axis.empty()) {
            axis = "y";
        }
        const bool endFace =
            (axis == "x" && (face == 0 || face == 1)) ||
            (axis == "y" && (face == 2 || face == 3)) ||
            (axis == "z" && (face == 4 || face == 5));
        if (endFace && !block->textures.top.empty()) {
            return block->textures.top;
        }
        if (!endFace && !block->textures.side.empty()) {
            return block->textures.side;
        }
    }

    return block->textures.forFace(face);
}

// Get UV coordinates and biome tint from atlas (or fallback grid if atlas null)
static void getUV(uint32_t stateId, int face,
                  const mc::TextureAtlas* atlas,
                  float& u0, float& v0, float& u1, float& v1,
                  TintRGB& tint)
{
    tint = {255, 255, 255};

    if (atlas && atlas->isLoaded()) {
        const mc::BlockState* bs = mc::getBlockState(stateId);
        if (bs && bs->block) {
            std::string name = textureForStateFace(*bs, face);
            if (name.empty()) {
                if (bs->block->name == "water") {
                    name = "water_still";
                } else if (bs->block->name == "lava") {
                    name = "lava_still";
                }
            }
            if (!name.empty()) {
                const mc::AtlasUV* auv = atlas->uv(name);
                if (auv) {
                    u0 = auv->u0; v0 = auv->v0;
                    u1 = auv->u1; v1 = auv->v1;
                    tint = getTextureTint(name);
                    return;
                }
            }
        }
        const mc::AtlasUV& miss = atlas->missingUV();
        u0 = miss.u0; v0 = miss.v0; u1 = miss.u1; v1 = miss.v1;
        return;
    }

    // No atlas — placeholder 16×16 grid (stateId % 256)
    uint32_t tileIdx = stateId % 256;
    float s = 1.0f / 16.0f;
    u0 = (float)(tileIdx % 16) * s;
    v0 = (float)(tileIdx / 16) * s;
    u1 = u0 + s;
    v1 = v0 + s;
}

// ── SectionMesh ────────────────────────────────────────────────────────────────

void SectionMesh::destroy(IRenderDevice* dev) {
    if (vbo) { dev->destroyBuffer(vbo); vbo = nullptr; }
    if (ibo) { dev->destroyBuffer(ibo); ibo = nullptr; }
    uploaded = false;
}

// ── ChunkMesher ────────────────────────────────────────────────────────────────

bool ChunkMesher::shouldCull(const LevelChunk& chunk, const LevelChunk* neighbors[4],
                              int wx, int wy, int wz, uint32_t myStateId)
{
    if (wy < CHUNK_MIN_Y || wy >= CHUNK_MAX_Y) return true;

    int cx = wx >> 4, cz = wz >> 4;
    int chunkCx = chunk.pos().x, chunkCz = chunk.pos().z;

    uint32_t neighborState = 0;
    if (cx == chunkCx && cz == chunkCz) {
        neighborState = chunk.getBlock(wx, wy, wz);
    } else {
        const LevelChunk* nbr = nullptr;
        if      (cx == chunkCx + 1 && cz == chunkCz) nbr = neighbors[0];
        else if (cx == chunkCx - 1 && cz == chunkCz) nbr = neighbors[1];
        else if (cz == chunkCz + 1 && cx == chunkCx) nbr = neighbors[2];
        else if (cz == chunkCz - 1 && cx == chunkCx) nbr = neighbors[3];
        if (!nbr) return false;
        neighborState = nbr->getBlock(wx, wy, wz);
    }

    const mc::BlockState* nb = mc::getBlockState(neighborState);
    if (!nb || !nb->block) return false;

    // Cull faces between adjacent fluids of the same type
    const mc::BlockState* myState = mc::getBlockState(myStateId);
    if (myState && myState->isFluid() && nb->isFluid()) {
        if (myState->block == nb->block) {
            return true;
        }
    }

    // Explicitly bypass culling if the neighboring block is a plant block
    const std::string& name = nb->block->name;
    if (name == "short_grass" || name == "tall_grass" || name == "fern" || name == "large_fern" ||
        name == "short_dry_grass" || name == "tall_dry_grass" || name == "dead_bush" ||
        name == "dandelion" || name == "poppy" || name == "blue_orchid" || name == "allium" || name == "azure_bluet" ||
        name == "oxeye_daisy" || name == "cornflower" || name == "lily_of_the_valley" ||
        name == "red_tulip" || name == "orange_tulip" || name == "white_tulip" || name == "pink_tulip" ||
        name == "bush" || name == "firefly_bush" || name == "sweet_berry_bush" ||
        name == "sugar_cane" || name == "seagrass" || name == "tall_seagrass" ||
        name == "sea_pickle" || name == "kelp" || name == "kelp_plant" ||
        name == "pink_petals" || name == "wildflowers" || name == "leaf_litter" ||
        name == "vine" || name == "cave_vines" || name == "cave_vines_plant" ||
        name.find("coral") != std::string::npos) {
        return false;
    }
    // Cross/cutout plants never occlude an adjacent face.
    if (isCrossPlant(nb->block->name)) return false;

    return nb->isOpaque();
}

void ChunkMesher::emitFace(SectionMesh& mesh, float bx, float by, float bz,
                            int face, uint32_t stateId, uint8_t light,
                            const TextureAtlas* atlas)
{
    float u0, v0, u1, v1;
    TintRGB tint;
    getUV(stateId, face, atlas, u0, v0, u1, v1, tint);

    // Directional shading matching vanilla (top=full, bottom=dark, sides=medium)
    static const uint8_t SHADE[6] = {210, 210, 255, 128, 230, 230};

    uint32_t baseIdx = (uint32_t)mesh.vertices.size();
    for (int v = 0; v < 4; ++v) {
        ChunkVertex vtx;
        vtx.x = bx + FACE_VERTS[face][v][0];
        vtx.y = by + FACE_VERTS[face][v][1];
        vtx.z = bz + FACE_VERTS[face][v][2];
        vtx.u = u0 + UV_CORNERS[v][0] * (u1 - u0);
        vtx.v = v0 + UV_CORNERS[v][1] * (v1 - v0);

        uint8_t shade = (uint8_t)((uint32_t)SHADE[face] * light / 15);
        vtx.r = (uint8_t)((uint32_t)shade * tint.r / 255);
        vtx.g = (uint8_t)((uint32_t)shade * tint.g / 255);
        vtx.b = (uint8_t)((uint32_t)shade * tint.b / 255);
        vtx.a = 255;
        mesh.vertices.push_back(vtx);
    }
    mesh.indices.push_back(baseIdx + 0);
    mesh.indices.push_back(baseIdx + 1);
    mesh.indices.push_back(baseIdx + 2);
    mesh.indices.push_back(baseIdx + 0);
    mesh.indices.push_back(baseIdx + 2);
    mesh.indices.push_back(baseIdx + 3);
}

void ChunkMesher::emitCross(SectionMesh& mesh, float bx, float by, float bz,
                            uint32_t stateId, uint8_t light,
                            const TextureAtlas* atlas,
                            const std::string& texOverride)
{
    float u0, v0, u1, v1;
    TintRGB tint;
    if (!texOverride.empty() && atlas && atlas->isLoaded()) {
        const mc::AtlasUV* auv = atlas->uv(texOverride);
        if (auv) {
            u0 = auv->u0; v0 = auv->v0;
            u1 = auv->u1; v1 = auv->v1;
            tint = getTextureTint(texOverride);
        } else {
            const mc::AtlasUV& miss = atlas->missingUV();
            u0 = miss.u0; v0 = miss.v0; u1 = miss.u1; v1 = miss.v1;
            tint = {255, 255, 255};
        }
    } else {
        getUV(stateId, 0, atlas, u0, v0, u1, v1, tint);
    }

    uint8_t r = (uint8_t)((uint32_t)light * tint.r / 15);
    uint8_t g = (uint8_t)((uint32_t)light * tint.g / 15);
    uint8_t b = (uint8_t)((uint32_t)light * tint.b / 15);

    // Cross Billboard Vertices (CCW winding, double-sided)
    // Plane 1, Side A
    {
        uint32_t base = (uint32_t)mesh.vertices.size();
        mesh.vertices.push_back({bx + 0.1f, by + 0.0f, bz + 0.1f, u0, v1, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 0.0f, bz + 0.9f, u1, v1, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 1.0f, bz + 0.9f, u1, v0, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.1f, by + 1.0f, bz + 0.1f, u0, v0, r, g, b, 255});
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    }
    // Plane 1, Side B
    {
        uint32_t base = (uint32_t)mesh.vertices.size();
        mesh.vertices.push_back({bx + 0.1f, by + 1.0f, bz + 0.1f, u0, v0, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 1.0f, bz + 0.9f, u1, v0, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 0.0f, bz + 0.9f, u1, v1, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.1f, by + 0.0f, bz + 0.1f, u0, v1, r, g, b, 255});
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    }
    // Plane 2, Side A
    {
        uint32_t base = (uint32_t)mesh.vertices.size();
        mesh.vertices.push_back({bx + 0.1f, by + 0.0f, bz + 0.9f, u0, v1, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 0.0f, bz + 0.1f, u1, v1, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 1.0f, bz + 0.1f, u1, v0, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.1f, by + 1.0f, bz + 0.9f, u0, v0, r, g, b, 255});
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    }
    // Plane 2, Side B
    {
        uint32_t base = (uint32_t)mesh.vertices.size();
        mesh.vertices.push_back({bx + 0.1f, by + 1.0f, bz + 0.9f, u0, v0, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 1.0f, bz + 0.1f, u1, v0, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.9f, by + 0.0f, bz + 0.1f, u1, v1, r, g, b, 255});
        mesh.vertices.push_back({bx + 0.1f, by + 0.0f, bz + 0.9f, u0, v1, r, g, b, 255});
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    }
}

static bool resolveTexture(const TextureAtlas* atlas, const std::string& texture,
                           float& u0, float& v0, float& u1, float& v1,
                           TintRGB& tint) {
    tint = getTextureTint(texture);
    if (atlas && atlas->isLoaded()) {
        if (const mc::AtlasUV* auv = atlas->uv(texture)) {
            u0 = auv->u0; v0 = auv->v0;
            u1 = auv->u1; v1 = auv->v1;
            return true;
        }
        const mc::AtlasUV& miss = atlas->missingUV();
        u0 = miss.u0; v0 = miss.v0; u1 = miss.u1; v1 = miss.v1;
        tint = {255, 255, 255};
        return false;
    }
    // Avoid smearing an entire atlas over a special quad when the atlas was not
    // available during mesh construction. A tiny deterministic tile is much
    // less destructive and matches the regular cube fallback path.
    uint32_t h = 2166136261u;
    for (unsigned char c : texture) {
        h ^= c;
        h *= 16777619u;
    }
    constexpr float s = 1.0f / 16.0f;
    const uint32_t tileIdx = h % 256u;
    u0 = static_cast<float>(tileIdx % 16u) * s;
    v0 = static_cast<float>(tileIdx / 16u) * s;
    u1 = u0 + s;
    v1 = v0 + s;
    return true;
}

static void emitTexturedQuad(SectionMesh& mesh,
                             const std::array<std::array<float, 3>, 4>& corners,
                             const TextureAtlas* atlas,
                             const std::string& texture,
                             uint8_t light) {
    float u0, v0, u1, v1;
    TintRGB tint;
    resolveTexture(atlas, texture, u0, v0, u1, v1, tint);
    const uint8_t r = (uint8_t)((uint32_t)light * tint.r / 15);
    const uint8_t g = (uint8_t)((uint32_t)light * tint.g / 15);
    const uint8_t b = (uint8_t)((uint32_t)light * tint.b / 15);

    const uint32_t base = (uint32_t)mesh.vertices.size();
    for (int i = 0; i < 4; ++i) {
        mesh.vertices.push_back({
            corners[i][0], corners[i][1], corners[i][2],
            u0 + UV_CORNERS[i][0] * (u1 - u0),
            v0 + UV_CORNERS[i][1] * (v1 - v0),
            r, g, b, 255
        });
    }
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 3); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 0);
}

static void emitGroundPlant(SectionMesh& mesh, float bx, float by, float bz,
                            const TextureAtlas* atlas, const std::string& texture,
                            uint8_t light) {
    const float y = by + 0.0625f;
    emitTexturedQuad(mesh, {{
        {{bx + 0.0f, y, bz + 1.0f}},
        {{bx + 1.0f, y, bz + 1.0f}},
        {{bx + 1.0f, y, bz + 0.0f}},
        {{bx + 0.0f, y, bz + 0.0f}},
    }}, atlas, texture, light);
}

// Segment-aware renderer for leaf_litter (LeafLitterBlock.java).
// Java creates 4 model variants (template_leaf_litter_1..4) via createLeafLitter /
// createSegmentedBlock; each shows N 8×8 half-quads at floor level, oriented by
// FACING. LEAF_LITTER_MODEL_2_SEGMENT_CONDITION covers amounts {2,3} while
// LEAF_LITTER_MODEL_3_SEGMENT_CONDITION adds a 3rd quad for amount==3.
// The vanilla Y-rotation applied per facing is (x,z)→(1-z, x) per 90° step.
static void emitLeafLitter(SectionMesh& mesh, float bx, float by, float bz,
                            const TextureAtlas* atlas, uint8_t light,
                            const mc::BlockState* bs) {
    int amount = 1;
    std::string facing = "north";
    if (bs) {
        const std::string aStr = bs->getProperty("segment_amount");
        if (!aStr.empty()) {
            try { amount = std::stoi(aStr); } catch (...) {}
        }
        const std::string fStr = bs->getProperty("facing");
        if (!fStr.empty()) facing = fStr;
    }
    amount = std::max(1, std::min(4, amount));

    const float y = by + 0.0625f;

    // Base 8×8 quadrant positions for FACING=NORTH (template_leaf_litter_N)
    struct HalfQuad { float x0, z0, x1, z1; };
    static const HalfQuad BASE[4] = {
        { 0.5f, 0.0f, 1.0f, 0.5f }, // segment 1: NE
        { 0.0f, 0.0f, 0.5f, 0.5f }, // segment 2: NW
        { 0.0f, 0.5f, 0.5f, 1.0f }, // segment 3: SW
        { 0.5f, 0.5f, 1.0f, 1.0f }, // segment 4: SE
    };

    int rotSteps = 0;
    if (facing == "east")       rotSteps = 1;
    else if (facing == "south") rotSteps = 2;
    else if (facing == "west")  rotSteps = 3;

    auto rotXZ = [rotSteps](float x, float z) -> std::pair<float, float> {
        for (int r = 0; r < rotSteps; ++r) { float nx = 1.0f - z; z = x; x = nx; }
        return { x, z };
    };

    for (int i = 0; i < amount; ++i) {
        const HalfQuad& q = BASE[i];
        // Corner order matches emitGroundPlant / UV_CORNERS: BL, BR, TR, TL
        float cx[4] = { q.x0, q.x1, q.x1, q.x0 };
        float cz[4] = { q.z1, q.z1, q.z0, q.z0 };
        std::array<std::array<float, 3>, 4> corners;
        for (int v = 0; v < 4; ++v) {
            auto [rx, rz] = rotXZ(cx[v], cz[v]);
            corners[v] = { bx + rx, y, bz + rz };
        }
        emitTexturedQuad(mesh, corners, atlas, "leaf_litter", light);
    }
}

static void emitVineFace(SectionMesh& mesh, float bx, float by, float bz,
                         const TextureAtlas* atlas, uint8_t light, int dir) {
    constexpr float kInset = 0.05f;
    if (dir == 0) { // north
        const float z = bz + kInset;
        emitTexturedQuad(mesh, {{{{bx, by, z}}, {{bx + 1.0f, by, z}}, {{bx + 1.0f, by + 1.0f, z}}, {{bx, by + 1.0f, z}}}}, atlas, "vine", light);
    } else if (dir == 1) { // east
        const float x = bx + 1.0f - kInset;
        emitTexturedQuad(mesh, {{{{x, by, bz}}, {{x, by, bz + 1.0f}}, {{x, by + 1.0f, bz + 1.0f}}, {{x, by + 1.0f, bz}}}}, atlas, "vine", light);
    } else if (dir == 2) { // south
        const float z = bz + 1.0f - kInset;
        emitTexturedQuad(mesh, {{{{bx + 1.0f, by, z}}, {{bx, by, z}}, {{bx, by + 1.0f, z}}, {{bx + 1.0f, by + 1.0f, z}}}}, atlas, "vine", light);
    } else { // west
        const float x = bx + kInset;
        emitTexturedQuad(mesh, {{{{x, by, bz + 1.0f}}, {{x, by, bz}}, {{x, by + 1.0f, bz}}, {{x, by + 1.0f, bz + 1.0f}}}}, atlas, "vine", light);
    }
}

namespace vanilla_model {

struct Face {
    std::string texture;
    std::string cullface;
    float uv[4] = {0.f, 0.f, 16.f, 16.f};
    bool hasUv = false;
};

struct Element {
    float from[3] = {0.f, 0.f, 0.f};
    float to[3] = {16.f, 16.f, 16.f};
    std::unordered_map<std::string, Face> faces;
};

struct Model {
    std::unordered_map<std::string, std::string> textures;
    std::vector<Element> elements;
    bool loaded = false;
};

std::mutex& cacheMutex() {
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, nlohmann::json>& jsonCache() {
    static std::unordered_map<std::string, nlohmann::json> c;
    return c;
}

std::unordered_map<std::string, Model>& modelCache() {
    static std::unordered_map<std::string, Model> c;
    return c;
}

std::string stripMinecraftNamespace(std::string id) {
    if (id.starts_with("minecraft:")) id.erase(0, 10);
    return id;
}

std::string normalizeTexture(std::string id) {
    id = stripMinecraftNamespace(std::move(id));
    if (id.starts_with("block/")) id.erase(0, 6);
    if (id.starts_with("textures/block/")) id.erase(0, 15);
    if (id.ends_with(".png")) id.resize(id.size() - 4);
    return id;
}

std::vector<uint8_t> readAssetOrLocal(const std::string& path) {
    if (auto bytes = mc::AssetManager::instance().readRaw(path); !bytes.empty()) {
        return bytes;
    }
    std::ifstream in("assets/client-extract/assets/" + path, std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

std::optional<nlohmann::json> loadJson(const std::string& path) {
    std::lock_guard<std::mutex> lock(cacheMutex());
    auto it = jsonCache().find(path);
    if (it != jsonCache().end()) return it->second;
    const std::vector<uint8_t> bytes = readAssetOrLocal(path);
    if (bytes.empty()) return std::nullopt;
    try {
        nlohmann::json j = nlohmann::json::parse(bytes.begin(), bytes.end());
        jsonCache()[path] = j;
        return j;
    } catch (...) {
        return std::nullopt;
    }
}

std::string modelAssetPath(std::string id) {
    id = stripMinecraftNamespace(std::move(id));
    return "minecraft/models/" + id + ".json";
}

bool readVec3(const nlohmann::json& j, const char* key, float out[3]) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() != 3) return false;
    for (int i = 0; i < 3; ++i) out[i] = (*it)[i].get<float>();
    return true;
}

Model loadModel(const std::string& id, std::unordered_set<std::string>& visiting) {
    {
        std::lock_guard<std::mutex> lock(cacheMutex());
        auto it = modelCache().find(id);
        if (it != modelCache().end()) return it->second;
    }
    if (!visiting.insert(id).second) return {};

    Model result;
    auto jsonOpt = loadJson(modelAssetPath(id));
    if (!jsonOpt) return {};
    const nlohmann::json& j = *jsonOpt;

    if (auto pit = j.find("parent"); pit != j.end() && pit->is_string()) {
        result = loadModel(pit->get<std::string>(), visiting);
    }
    if (auto tit = j.find("textures"); tit != j.end() && tit->is_object()) {
        for (const auto& item : tit->items()) {
            if (item.value().is_string()) result.textures[item.key()] = item.value().get<std::string>();
        }
    }
    if (auto eit = j.find("elements"); eit != j.end() && eit->is_array()) {
        result.elements.clear();
        for (const auto& ejson : *eit) {
            Element e;
            readVec3(ejson, "from", e.from);
            readVec3(ejson, "to", e.to);
            if (auto fit = ejson.find("faces"); fit != ejson.end() && fit->is_object()) {
                for (const auto& fitem : fit->items()) {
                    Face f;
                    const auto& fj = fitem.value();
                    f.texture = fj.value("texture", "");
                    f.cullface = fj.value("cullface", "");
                    if (auto uit = fj.find("uv"); uit != fj.end() && uit->is_array() && uit->size() == 4) {
                        for (int i = 0; i < 4; ++i) f.uv[i] = (*uit)[i].get<float>();
                        f.hasUv = true;
                    }
                    e.faces[fitem.key()] = f;
                }
            }
            result.elements.push_back(std::move(e));
        }
    }
    result.loaded = !result.elements.empty();

    {
        std::lock_guard<std::mutex> lock(cacheMutex());
        modelCache()[id] = result;
    }
    return result;
}

std::string resolveTextureRef(const Model& model, std::string ref) {
    std::unordered_set<std::string> seen;
    while (!ref.empty() && ref[0] == '#') {
        ref.erase(0, 1);
        if (!seen.insert(ref).second) return "missingno";
        auto it = model.textures.find(ref);
        if (it == model.textures.end()) return "missingno";
        ref = it->second;
    }
    return ref.empty() ? "missingno" : normalizeTexture(ref);
}

bool matchesWhen(const nlohmann::json& when, const mc::BlockState& state) {
    if (!when.is_object()) return true;
    if (auto orIt = when.find("OR"); orIt != when.end() && orIt->is_array()) {
        for (const auto& child : *orIt) if (matchesWhen(child, state)) return true;
        return false;
    }
    if (auto andIt = when.find("AND"); andIt != when.end() && andIt->is_array()) {
        for (const auto& child : *andIt) if (!matchesWhen(child, state)) return false;
        return true;
    }
    for (const auto& item : when.items()) {
        if (!item.value().is_string()) continue;
        const std::string actual = state.getProperty(item.key());
        bool ok = false;
        std::string allowed = item.value().get<std::string>();
        size_t start = 0;
        while (start <= allowed.size()) {
            size_t bar = allowed.find('|', start);
            if (bar == std::string::npos) bar = allowed.size();
            if (actual == allowed.substr(start, bar - start)) { ok = true; break; }
            start = bar + 1;
        }
        if (!ok) return false;
    }
    return true;
}

bool selectorMatches(const std::string& selector, const mc::BlockState& state) {
    if (selector.empty()) return true;
    size_t start = 0;
    while (start < selector.size()) {
        size_t comma = selector.find(',', start);
        if (comma == std::string::npos) comma = selector.size();
        size_t eq = selector.find('=', start);
        if (eq == std::string::npos || eq > comma) return false;
        std::string key = selector.substr(start, eq - start);
        std::string allowed = selector.substr(eq + 1, comma - eq - 1);
        const std::string actual = state.getProperty(key);
        bool ok = false;
        size_t vstart = 0;
        while (vstart <= allowed.size()) {
            size_t bar = allowed.find('|', vstart);
            if (bar == std::string::npos) bar = allowed.size();
            if (actual == allowed.substr(vstart, bar - vstart)) { ok = true; break; }
            vstart = bar + 1;
        }
        if (!ok) return false;
        start = comma + 1;
    }
    return true;
}

void collectApplyModels(const nlohmann::json& apply, std::vector<std::string>& out) {
    if (apply.is_array()) {
        if (!apply.empty()) collectApplyModels(apply.front(), out);
        return;
    }
    if (apply.is_object()) {
        std::string model = apply.value("model", "");
        if (!model.empty()) out.push_back(model);
    }
}

std::vector<std::string> modelsForState(const mc::BlockState& state) {
    std::vector<std::string> out;
    if (!state.block) return out;
    auto jOpt = loadJson("minecraft/blockstates/" + state.block->name + ".json");
    if (!jOpt) return out;
    const nlohmann::json& j = *jOpt;

    if (auto vit = j.find("variants"); vit != j.end() && vit->is_object()) {
        if (auto exact = vit->find(state.props); exact != vit->end()) {
            collectApplyModels(*exact, out);
            return out;
        }
        if (auto empty = vit->find(""); empty != vit->end()) {
            collectApplyModels(*empty, out);
            return out;
        }
        for (const auto& item : vit->items()) {
            if (selectorMatches(item.key(), state)) {
                collectApplyModels(item.value(), out);
                return out;
            }
        }
    }
    if (auto mit = j.find("multipart"); mit != j.end() && mit->is_array()) {
        for (const auto& part : *mit) {
            const auto wit = part.find("when");
            if (wit == part.end() || matchesWhen(*wit, state)) {
                if (auto ait = part.find("apply"); ait != part.end()) collectApplyModels(*ait, out);
            }
        }
    }
    return out;
}

int faceIndex(const std::string& name) {
    for (int i = 0; i < 6; ++i) if (name == FACE_NAMES[i]) return i;
    return -1;
}

void defaultFaceUv(const Element& e, int face, float uv[4]) {
    const float* f = e.from;
    const float* t = e.to;
    switch (face) {
        case 3: uv[0] = f[0];        uv[1] = 16.f - t[2]; uv[2] = t[0];        uv[3] = 16.f - f[2]; break;
        case 2: uv[0] = f[0];        uv[1] = f[2];        uv[2] = t[0];        uv[3] = t[2];        break;
        case 5: uv[0] = 16.f - t[0]; uv[1] = 16.f - t[1]; uv[2] = 16.f - f[0]; uv[3] = 16.f - f[1]; break;
        case 4: uv[0] = f[0];        uv[1] = 16.f - t[1]; uv[2] = t[0];        uv[3] = 16.f - f[1]; break;
        case 1: uv[0] = f[2];        uv[1] = 16.f - t[1]; uv[2] = t[2];        uv[3] = 16.f - f[1]; break;
        case 0: uv[0] = 16.f - t[2]; uv[1] = 16.f - t[1]; uv[2] = 16.f - f[2]; uv[3] = 16.f - f[1]; break;
        default: uv[0] = 0.f; uv[1] = 0.f; uv[2] = 16.f; uv[3] = 16.f; break;
    }
}

std::array<std::array<float, 3>, 4> faceCorners(const Element& e, float bx, float by, float bz, int face) {
    const float x0 = bx + e.from[0] / 16.f, y0 = by + e.from[1] / 16.f, z0 = bz + e.from[2] / 16.f;
    const float x1 = bx + e.to[0] / 16.f,   y1 = by + e.to[1] / 16.f,   z1 = bz + e.to[2] / 16.f;
    switch (face) {
        case 0: return {{{{x1,y0,z1}}, {{x1,y1,z1}}, {{x1,y1,z0}}, {{x1,y0,z0}}}};
        case 1: return {{{{x0,y0,z0}}, {{x0,y1,z0}}, {{x0,y1,z1}}, {{x0,y0,z1}}}};
        case 2: return {{{{x0,y1,z0}}, {{x1,y1,z0}}, {{x1,y1,z1}}, {{x0,y1,z1}}}};
        case 3: return {{{{x0,y0,z1}}, {{x1,y0,z1}}, {{x1,y0,z0}}, {{x0,y0,z0}}}};
        case 4: return {{{{x0,y0,z1}}, {{x0,y1,z1}}, {{x1,y1,z1}}, {{x1,y0,z1}}}};
        default:return {{{{x1,y0,z0}}, {{x1,y1,z0}}, {{x0,y1,z0}}, {{x0,y0,z0}}}};
    }
}

void emitModelFace(SectionMesh& mesh,
                   const Element& e,
                   const Face& f,
                   const Model& model,
                   int face,
                   float bx, float by, float bz,
                   const TextureAtlas* atlas,
                   uint8_t light) {
    const std::string texture = resolveTextureRef(model, f.texture);
    float u0, v0, u1, v1;
    TintRGB tint;
    resolveTexture(atlas, texture, u0, v0, u1, v1, tint);

    float uv[4];
    if (f.hasUv) std::copy(std::begin(f.uv), std::end(f.uv), uv);
    else defaultFaceUv(e, face, uv);

    const uint8_t shade = (face == 2) ? 255 : (face == 3 ? 128 : (face == 0 || face == 1 ? 210 : 230));
    const uint8_t r = (uint8_t)((uint32_t)shade * light * tint.r / (15u * 255u));
    const uint8_t g = (uint8_t)((uint32_t)shade * light * tint.g / (15u * 255u));
    const uint8_t b = (uint8_t)((uint32_t)shade * light * tint.b / (15u * 255u));

    const float uu[4] = { uv[0], uv[0], uv[2], uv[2] };
    const float vv[4] = { uv[1], uv[3], uv[3], uv[1] };
    auto corners = faceCorners(e, bx, by, bz, face);
    const uint32_t base = (uint32_t)mesh.vertices.size();
    for (int i = 0; i < 4; ++i) {
        mesh.vertices.push_back({
            corners[i][0], corners[i][1], corners[i][2],
            u0 + (uu[i] / 16.f) * (u1 - u0),
            v0 + (vv[i] / 16.f) * (v1 - v0),
            r, g, b, 255
        });
    }
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 1); mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0); mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
}

bool tryEmitVanillaBlockModel(SectionMesh& mesh,
                              const LevelChunk& chunk,
                              const LevelChunk* neighbors[4],
                              int wx, int wy, int wz,
                              const mc::BlockState& state,
                              uint32_t stateId,
                              uint8_t light,
                              const TextureAtlas* atlas) {
    const std::vector<std::string> modelIds = modelsForState(state);
    if (modelIds.empty()) return false;

    bool emitted = false;
    std::unordered_set<std::string> visiting;
    for (const std::string& modelId : modelIds) {
        visiting.clear();
        const Model model = loadModel(modelId, visiting);
        if (!model.loaded) continue;
        for (const Element& e : model.elements) {
            for (const auto& [faceName, face] : e.faces) {
                int fi = faceIndex(faceName);
                if (fi < 0) continue;
                if (!face.cullface.empty()) {
                    const int ci = faceIndex(face.cullface);
                    if (ci >= 0 && ChunkMesher::shouldCull(chunk, neighbors, wx + DX[ci], wy + DY[ci], wz + DZ[ci], stateId)) {
                        continue;
                    }
                }
                emitModelFace(mesh, e, face, model, fi, (float)wx, (float)wy, (float)wz, atlas, light);
                emitted = true;
            }
        }
    }
    return emitted;
}

} // namespace vanilla_model

void ChunkMesher::buildSection(const LevelChunk& chunk, int sectionIndex,
                                const LevelChunk* neighbors[4],
                                const TextureAtlas* atlas,
                                SectionMesh& out)
{
    out.clear();
    const ChunkSection* sec = chunk.getSection(sectionIndex);
    if (!sec || sec->isEmpty()) return;

    int baseY = CHUNK_MIN_Y + sectionIndex * 16;
    int chunkWorldX = chunk.pos().x * 16;
    int chunkWorldZ = chunk.pos().z * 16;
    auto stateAt = [&](int wx, int wy, int wz) -> uint32_t {
        const int cx = wx >> 4;
        const int cz = wz >> 4;
        const int chunkCx = chunk.pos().x;
        const int chunkCz = chunk.pos().z;
        if (cx == chunkCx && cz == chunkCz) {
            return chunk.getBlock(wx, wy, wz);
        }
        const LevelChunk* nbr = nullptr;
        if      (cx == chunkCx + 1 && cz == chunkCz) nbr = neighbors[0];
        else if (cx == chunkCx - 1 && cz == chunkCz) nbr = neighbors[1];
        else if (cz == chunkCz + 1 && cx == chunkCx) nbr = neighbors[2];
        else if (cz == chunkCz - 1 && cx == chunkCx) nbr = neighbors[3];
        return nbr ? nbr->getBlock(wx, wy, wz) : 0;
    };
    auto solidAt = [&](int wx, int wy, int wz) {
        const mc::BlockState* s = mc::getBlockState(stateAt(wx, wy, wz));
        return s && s->block && s->block->isSolid();
    };

    for (int ly = 0; ly < 16; ++ly) {
        int wy = baseY + ly;
        for (int lz = 0; lz < 16; ++lz) {
            int wz = chunkWorldZ + lz;
            for (int lx = 0; lx < 16; ++lx) {
                int wx = chunkWorldX + lx;

                uint32_t stateId = sec->getBlock(lx, ly, lz);
                if (stateId == 0) continue;

                const mc::BlockState* bs = mc::getBlockState(stateId);
                if (!bs || bs->isAir()) continue;

                float bx = (float)wx, by = (float)wy, bz = (float)wz;

                uint8_t light = sec->getSkyLight(lx, ly, lz);
                if (light == 0) light = sec->getBlockLight(lx, ly, lz);
                if (light == 0) {
                    // Server didn't send light — synthesize from heightmap:
                    // blocks at-or-above the heightmap top get full skylight,
                    // blocks deeper fall off linearly to a cave floor of 3.
                    int16_t topY = chunk.heightmap(lx, lz);
                    int depth = topY - wy;
                    if      (depth <= 0)  light = 15;
                    else if (depth >= 12) light = 3;
                    else                  light = (uint8_t)(15 - depth);
                }

                bool isPlant = false;
                if (bs && bs->block) {
                    const std::string& name = bs->block->name;
                    if (vanilla_model::tryEmitVanillaBlockModel(out, chunk, neighbors, wx, wy, wz, *bs, stateId, light, atlas)) {
                        continue;
                    }
                    if (name == "pink_petals" || name == "wildflowers") {
                        emitGroundPlant(out, bx, by, bz, atlas, name, light);
                        continue;
                    }
                    if (name == "leaf_litter") {
                        emitLeafLitter(out, bx, by, bz, atlas, light, bs);
                        continue;
                    }
                    if (name == "vine") {
                        bool emitted = false;
                        const int support[4][2] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };
                        for (int dir = 0; dir < 4; ++dir) {
                            if (solidAt(wx + support[dir][0], wy, wz + support[dir][1])) {
                                emitVineFace(out, bx, by, bz, atlas, light, dir);
                                emitted = true;
                            }
                        }
                        if (!emitted) {
                            emitVineFace(out, bx, by, bz, atlas, light, 0);
                        }
                        continue;
                    }
                    isPlant = isCrossPlant(name);
                }

                if (isPlant) {
                    std::string texOverride = "";
                    if (bs && bs->block) {
                        const std::string& name = bs->block->name;
                        if (name == "tall_grass") {
                            bool hasAbove = false;
                            if (wy + 1 < CHUNK_MAX_Y) {
                                uint32_t stateIdAbove = chunk.getBlock(wx, wy + 1, wz);
                                const mc::BlockState* bsAbove = mc::getBlockState(stateIdAbove);
                                if (bsAbove && bsAbove->block && bsAbove->block->name == "tall_grass") {
                                    hasAbove = true;
                                }
                            }
                            if (hasAbove) {
                                texOverride = "tall_grass_bottom";
                            } else {
                                texOverride = "tall_grass_top";
                            }
                        } else if (name == "large_fern") {
                            bool hasAbove = false;
                            if (wy + 1 < CHUNK_MAX_Y) {
                                uint32_t stateIdAbove = chunk.getBlock(wx, wy + 1, wz);
                                const mc::BlockState* bsAbove = mc::getBlockState(stateIdAbove);
                                if (bsAbove && bsAbove->block && bsAbove->block->name == "large_fern") {
                                    hasAbove = true;
                                }
                            }
                            if (hasAbove) {
                                texOverride = "large_fern_bottom";
                            } else {
                                texOverride = "large_fern_top";
                            }
                        }
                    }
                    emitCross(out, bx, by, bz, stateId, light, atlas, texOverride);
                } else {
                    for (int face = 0; face < 6; ++face) {
                        int nwx = wx + DX[face];
                        int nwy = wy + DY[face];
                        int nwz = wz + DZ[face];
                        if (!shouldCull(chunk, neighbors, nwx, nwy, nwz, stateId))
                            emitFace(out, bx, by, bz, face, stateId, light, atlas);
                    }
                }
            }
        }
    }
}

std::vector<SectionMesh> ChunkMesher::buildChunk(const LevelChunk& chunk,
                                                   const LevelChunk* neighbors[4],
                                                   const TextureAtlas* atlas)
{
    std::vector<SectionMesh> meshes(CHUNK_SECTION_COUNT);
    for (int s = 0; s < CHUNK_SECTION_COUNT; ++s)
        buildSection(chunk, s, neighbors, atlas, meshes[s]);
    return meshes;
}

} // namespace mc::render
