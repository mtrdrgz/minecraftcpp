#include "ChunkMesh.h"
#include "../../world/level/block/BlockState.h"
#include "../../world/level/block/Blocks.h"
#include "../../core/Log.h"
#include <array>
#include <algorithm>

namespace mc::render {

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
        name == "sugar_cane")
        return {121, 192, 90};   // #79C05A plains grass
    if (name == "water_still" || name == "water_flow")
        return {63, 118, 228};   // #3F76E4 beautiful water blue
    // Fixed spruce leaf tint (biome-independent per BlockColors.java)
    if (name == "spruce_leaves") return {97,  153, 97};  // #619961
    // Fixed birch leaf tint (biome-independent per BlockColors.java)
    if (name == "birch_leaves")  return {128, 167, 85};  // #80A755
    // Remaining leaves & vines use biome foliage color
    if (name.ends_with("_leaves") || name == "vine") return {89, 174, 48}; // #59AE30
    return {255, 255, 255};
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
            std::string name = bs->block->textures.forFace(face);
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
    if (name == "short_grass" || name == "tall_grass" || name == "fern" || name == "large_fern" || name == "dead_bush" ||
        name == "dandelion" || name == "poppy" || name == "blue_orchid" || name == "allium" || name == "azure_bluet" ||
        name == "oxeye_daisy" || name == "cornflower" || name == "lily_of_the_valley" ||
        name == "red_tulip" || name == "orange_tulip" || name == "white_tulip" || name == "pink_tulip" ||
        name == "sweet_berry_bush" || name == "sugar_cane" || name == "seagrass" || name == "tall_seagrass" ||
        name == "sea_pickle" || name == "kelp" || name == "kelp_plant" ||
        name.find("coral") != std::string::npos) {
        return false;
    }

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
                    isPlant = (name == "short_grass" || name == "tall_grass" || name == "fern" || name == "large_fern" || name == "dead_bush" ||
                               name == "dandelion" || name == "poppy" || name == "blue_orchid" || name == "allium" || name == "azure_bluet" ||
                               name == "oxeye_daisy" || name == "cornflower" || name == "lily_of_the_valley" ||
                               name == "red_tulip" || name == "orange_tulip" || name == "white_tulip" || name == "pink_tulip" ||
                               name == "sweet_berry_bush" || name == "sugar_cane" || name == "seagrass" || name == "tall_seagrass" ||
                               name == "sea_pickle" || name == "kelp" || name == "kelp_plant" ||
                               name.find("coral") != std::string::npos);
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
