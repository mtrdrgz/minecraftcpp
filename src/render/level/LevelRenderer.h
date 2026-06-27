#pragma once
#include "ChunkMesh.h"
#include "../../client/Minecraft.h"
#include "../../platform/Window.h"
#include "../../assets/TextureAtlas.h"
#include "../../core/ThreadPool.h"
#include "../../world/level/biome/BiomeRegistry.h"
#include "../entity/EntityRenderDispatcher.h"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <future>
#include <chrono>
#include <glm/glm.hpp>

namespace mc::render {

// Port of net.minecraft.client.renderer.LevelRenderer
class LevelRenderer {
public:
    LevelRenderer(IRenderDevice* device, Minecraft* mc, Window* window);
    ~LevelRenderer();

    // Called every frame — processes input, rebuilds dirty chunk meshes, renders
    void renderLevel(ICommandList* cmd, float partialTick);

    void onChunkLoaded(ChunkPos pos);

private:
    void rebuildDirtyChunks();
    void uploadAndDrawSection(ICommandList* cmd, SectionMesh& mesh);
    void setupChunkPipeline();
    void setupSkyPipeline();
    void setupHudPipeline();
    void loadAtlas(ICommandList* cmd);
    void updateCamera(float dtSec);
    void renderSky(ICommandList* cmd, const glm::mat4& invVP, int winW, int winH);
    void renderHud(ICommandList* cmd, int winW, int winH);

    IRenderDevice* m_device;
    Minecraft*     m_mc;
    Window*        m_window;

    IPipeline*     m_pipeline    = nullptr;
    IPipeline*     m_skyPipeline = nullptr;
    IPipeline*     m_hudPipeline = nullptr;
    // Crosshair geometry
    IBuffer*       m_hudVbo      = nullptr;
    IBuffer*       m_hudIbo      = nullptr;
    // Hotbar background (182×22 quad)
    IBuffer*       m_hotbarBgVbo = nullptr;
    IBuffer*       m_hotbarBgIbo = nullptr;
    // Hotbar selection highlight (24×24 quad over slot 0)
    IBuffer*       m_hotbarSelVbo = nullptr;
    IBuffer*       m_hotbarSelIbo = nullptr;
    TextureAtlas   m_atlas;
    bool           m_atlasLoading = false;
    std::future<void> m_atlasStitchFuture;
    std::unique_ptr<EntityRenderDispatcher> m_entityRenderer;

    // Per-biome block colouring (grass/foliage/water). Loaded once when a world is
    // active; passed to the mesh worker as an immutable per-chunk BiomeMeshContext.
    std::shared_ptr<const std::vector<std::int32_t>> m_grassColormap;
    std::shared_ptr<const std::vector<std::int32_t>> m_foliageColormap;
    std::unique_ptr<mc::biome::BiomeRegistry>        m_biomeRegistry;
    bool m_biomeDataLoaded = false;
    void ensureBiomeData();
    std::shared_ptr<BiomeMeshContext> makeBiomeContext(ChunkPos cp);

    // Free-fly camera (decoupled from server player position after first sync)
    glm::vec3 m_camPos{0.f, 80.f, 0.f};
    float     m_camYaw   = 0.f;   // degrees, around Y axis
    float     m_camPitch = 0.f;   // degrees, up/down
    bool      m_camInitialized = false;

    // Frame timing for camera movement
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_lastFrame = Clock::now();
    int m_sectionUploadsThisFrame = 0;

    struct ChunkRenderData {
        std::vector<SectionMesh> sections;
        bool built = false;
    };
    std::unordered_map<int64_t, ChunkRenderData> m_renderData;

    struct PendingMeshBuild {
        ChunkPos pos{};
        std::future<std::vector<SectionMesh>> future;
    };
    std::unique_ptr<ThreadPool> m_meshPool;
    size_t m_meshPoolSize = 1;  // cached for budget calculations
    std::vector<PendingMeshBuild> m_pendingMeshBuilds;
    std::unordered_set<int64_t> m_meshBuildQueued;

    static int64_t chunkKey(ChunkPos p) {
        return ((int64_t)(uint32_t)p.x << 32) | (uint32_t)p.z;
    }

    // ── GLSL shaders ───────────────────────────────────────────────────────────

    // Fullscreen triangle via gl_VertexID (no VBO needed)
    static constexpr const char* SKY_VS = R"(
#version 460 core
void main() {
    vec2 pos = vec2[3](vec2(-1,-1), vec2(3,-1), vec2(-1,3))[gl_VertexID];
    gl_Position = vec4(pos, 0.9999, 1.0);
}
)";

    // Procedural sky gradient (plains biome noon — no real sky dome yet)
    static constexpr const char* SKY_FS = R"(
#version 460 core
out vec4 fragColor;
uniform mat4  uInvVP;
uniform vec4  uScreen;

void main() {
    vec2  ndc    = (gl_FragCoord.xy / uScreen.xy) * 2.0 - 1.0;
    vec4  world  = uInvVP * vec4(ndc, 0.0, 1.0);
    vec3  dir    = normalize(world.xyz / world.w);
    float up     = clamp(dir.y, 0.0, 1.0);
    float low    = clamp(-dir.y, 0.0, 1.0);
    // Plains biome sky: zenith #78A7FF, horizon #C0D8FF, void #050505
    vec3 zenith  = vec3(0.471, 0.655, 1.000);
    vec3 horizon = vec3(0.752, 0.847, 1.000);
    vec3 sky     = mix(horizon, zenith, up);
    // Dark void below horizon
    sky = mix(sky, vec3(0.02), low * 0.7);
    fragColor = vec4(sky, 1.0);
}
)";

    static constexpr const char* CHUNK_VS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;

uniform mat4 uMVP;
uniform vec4 uCamPos;

out vec2  vUV;
out vec4  vColor;
out float vDist;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV    = aUV;
    vColor = aColor;
    vDist  = distance(aPos, uCamPos.xyz);
}
)";

    // 2D screen-space pipeline for HUD (crosshair, hotbar, eventually full GUI)
    static constexpr const char* HUD_VS = R"(
#version 460 core
layout(location=0) in vec2 aPix;       // pixel offset from element origin
uniform vec4 uHalfScreen;              // (width/2, height/2)
uniform vec4 uAnchor;                  // pixel offset of element origin from screen centre
void main() {
    gl_Position = vec4((aPix + uAnchor.xy) / uHalfScreen.xy, 0.0, 1.0);
}
)";

    static constexpr const char* HUD_FS = R"(
#version 460 core
out vec4 fragColor;
uniform vec4 uColor;
void main() { fragColor = uColor; }
)";

    static constexpr const char* CHUNK_FS = R"(
#version 460 core
in vec2  vUV;
in vec4  vColor;
in float vDist;
out vec4 fragColor;

uniform sampler2D uAtlas;
uniform vec4      uFogStart;
uniform vec4      uFogEnd;
uniform vec4      uFogColor;

void main() {
    vec4 tex = texture(uAtlas, vUV);
    if (tex.a < 0.1) discard;
    vec4 lit = tex * vColor;
    float fogT = clamp((vDist - uFogStart.x) / max(uFogEnd.x - uFogStart.x, 1.0), 0.0, 1.0);
    fragColor  = vec4(mix(lit.rgb, uFogColor.rgb, fogT), lit.a);
}
)";
};

} // namespace mc::render
