#include "ChunkMesh.h"
#include "../../world/level/block/BlockState.h"
#include "../../world/level/block/Blocks.h"
#include "../../core/Log.h"
#include <array>
#include <algorithm>
#include <string>
#include <unordered_set>

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
