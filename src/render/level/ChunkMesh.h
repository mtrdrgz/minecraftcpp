#pragma once
#include "../../world/level/chunk/LevelChunk.h"
#include "../../assets/TextureAtlas.h"
#include "../IRenderDevice.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace mc::render {

// Per-vertex data for chunk geometry
struct ChunkVertex {
    float x, y, z;        // world position
    float u, v;           // texture UV (into atlas)
    uint8_t r, g, b, a;   // light / AO color
};

// A built mesh for one chunk section
struct SectionMesh {
    std::vector<ChunkVertex> vertices;
    std::vector<uint32_t>    indices;

    IBuffer* vbo = nullptr;
    IBuffer* ibo = nullptr;
    bool     uploaded = false;

    void clear() { vertices.clear(); indices.clear(); }
    bool empty()  const { return vertices.empty(); }
    void destroy(IRenderDevice* dev);
};

// Builds chunk section meshes by iterating blocks and emitting quads.
// Port of net.minecraft.client.renderer.chunk.SectionCompiler
class ChunkMesher {
public:
    // Build meshes for all non-empty sections in a chunk.
    // atlas may be nullptr — falls back to a placeholder UV grid.
    static std::vector<SectionMesh> buildChunk(const LevelChunk& chunk,
                                                const LevelChunk* neighbors[4],
                                                const TextureAtlas* atlas);

    static bool shouldCull(const LevelChunk& chunk, const LevelChunk* neighbors[4],
                            int wx, int wy, int wz,
                            uint32_t myStateId);

private:
    static void buildSection(const LevelChunk& chunk, int sectionIndex,
                              const LevelChunk* neighbors[4],
                              const TextureAtlas* atlas,
                              SectionMesh& out);

    static void emitFace(SectionMesh& mesh,
                         float bx, float by, float bz,
                         int face,
                         uint32_t stateId,
                         uint8_t light,
                         const TextureAtlas* atlas);

    static void emitCross(SectionMesh& mesh,
                          float bx, float by, float bz,
                          uint32_t stateId,
                          uint8_t light,
                          const TextureAtlas* atlas,
                          const std::string& texOverride = "");
};

} // namespace mc::render
