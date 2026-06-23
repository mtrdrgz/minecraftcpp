#include "LevelRenderer.h"
#include "../../world/level/block/Blocks.h"
#include "../../world/level/biome/BiomeColor.h"
#include "../../assets/resource_ids.h"
#include "../../core/Log.h"
#include "../../assets/AssetManager.h"
#include "../../../profiling/include/Profiler.h"
#include <stb_image.h>
#include <fstream>
#include <iterator>
#include <string>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include "../../platform/Platform.h"
#endif

namespace mc::render {

namespace {
    // Port of Entity.calculateViewVector(xRot, yRot): yaw is negated and pitch
    // contributes a negative Y component. Keeping the renderer on the same
    // convention prevents the generated world from appearing mirrored/flipped.
    glm::vec3 vanillaViewVector(float xRot, float yRot) {
        const float realXRot = glm::radians(xRot);
        const float realYRot = glm::radians(-yRot);
        const float yCos = cosf(realYRot);
        const float ySin = sinf(realYRot);
        const float xCos = cosf(realXRot);
        const float xSin = sinf(realXRot);
        return { ySin * xCos, -xSin, yCos * xCos };
    }
}

LevelRenderer::LevelRenderer(IRenderDevice* device, Minecraft* mc, Window* window)
    : m_device(device), m_mc(mc), m_window(window)
{
    m_meshPool = std::make_unique<ThreadPool>(1);
    setupChunkPipeline();
    setupSkyPipeline();
    setupHudPipeline();
    m_entityRenderer = std::make_unique<EntityRenderDispatcher>(device);
    // Atlas load needs a command list, so we defer it to the first render pass
}

LevelRenderer::~LevelRenderer() {
    m_pendingMeshBuilds.clear();
    m_meshBuildQueued.clear();
    m_meshPool.reset();
    for (auto& [key, rd] : m_renderData)
        for (auto& mesh : rd.sections)
            mesh.destroy(m_device);
    if (m_pipeline)    m_device->destroyPipeline(m_pipeline);
    if (m_skyPipeline) m_device->destroyPipeline(m_skyPipeline);
    if (m_hudPipeline) m_device->destroyPipeline(m_hudPipeline);
    if (m_hudVbo)      m_device->destroyBuffer(m_hudVbo);
    if (m_hudIbo)      m_device->destroyBuffer(m_hudIbo);
    if (m_hotbarBgVbo) m_device->destroyBuffer(m_hotbarBgVbo);
    if (m_hotbarBgIbo) m_device->destroyBuffer(m_hotbarBgIbo);
    if (m_hotbarSelVbo) m_device->destroyBuffer(m_hotbarSelVbo);
    if (m_hotbarSelIbo) m_device->destroyBuffer(m_hotbarSelIbo);
    if (m_atlas.isLoaded()) m_device->destroyTexture(m_atlas.texture());
}

void LevelRenderer::setupChunkPipeline() {
    PipelineDesc desc;
    desc.vsSource  = CHUNK_VS;
    desc.fsSource  = CHUNK_FS;
    desc.depth     = DepthTest::ReadWrite;
    desc.cull      = CullMode::Back;
    desc.blend     = BlendMode::None;
    desc.layout    = VertexLayout::PositionTexColor;
    m_pipeline = m_device->createPipeline(desc);
}

void LevelRenderer::setupSkyPipeline() {
    PipelineDesc desc;
    desc.vsSource = SKY_VS;
    desc.fsSource = SKY_FS;
    desc.depth    = DepthTest::Disabled;
    desc.cull     = CullMode::None;
    desc.blend    = BlendMode::None;
    desc.layout    = VertexLayout::Simple;
    m_skyPipeline = m_device->createPipeline(desc);
}

void LevelRenderer::renderSky(ICommandList* cmd, const glm::mat4& invVP, int winW, int winH) {
    if (!m_skyPipeline) return;
    cmd->bindPipeline(m_skyPipeline);
    cmd->setUniformMat4("uInvVP", glm::value_ptr(invVP));
    cmd->setUniform4f("uScreen", (float)winW, (float)winH, 0.0f, 0.0f);
    cmd->draw(3, 0); 
}

void LevelRenderer::setupHudPipeline() {
    PipelineDesc desc;
    desc.vsSource = HUD_VS;
    desc.fsSource = HUD_FS;
    desc.depth    = DepthTest::Disabled;
    desc.cull     = CullMode::None;
    desc.blend    = BlendMode::Alpha;
    desc.layout    = VertexLayout::Simple;
    m_hudPipeline = m_device->createPipeline(desc);

    m_hudVbo = m_device->createBuffer({24 * 4, BufferUsage::Vertex, false});
    m_hudIbo = m_device->createBuffer({12 * 4, BufferUsage::Index, false});
    m_hotbarBgVbo = m_device->createBuffer({12 * 4, BufferUsage::Vertex, false});
    m_hotbarBgIbo = m_device->createBuffer({6 * 4, BufferUsage::Index, false});
    m_hotbarSelVbo = m_device->createBuffer({12 * 4, BufferUsage::Vertex, false});
    m_hotbarSelIbo = m_device->createBuffer({6 * 4, BufferUsage::Index, false});
}

void LevelRenderer::renderHud(ICommandList* cmd, int winW, int winH) {
    if (!m_hudPipeline) return;
    
    static bool hudUploaded = false;
    if (!hudUploaded) {
        constexpr float SZ = 7.0f, TH = 1.0f;
        const float verts[] = { -SZ,-TH,0, SZ,-TH,0, SZ,TH,0, -SZ,TH,0, -TH,-SZ,0, TH,-SZ,0, TH,SZ,0, -TH,SZ,0 };
        const uint32_t idxs[] = { 0,1,2, 0,2,3, 4,5,6, 4,6,7 };
        cmd->uploadBuffer(m_hudVbo, verts, sizeof(verts));
        cmd->uploadBuffer(m_hudIbo, idxs, sizeof(idxs));
        
        constexpr float HW = 91.0f, HH = 11.0f;
        const float bg[] = { -HW,-HH,0, HW,-HH,0, HW,HH,0, -HW,HH,0 };
        cmd->uploadBuffer(m_hotbarBgVbo, bg, sizeof(bg));
        const uint32_t bgIdxs[] = { 0,1,2, 0,2,3 };
        cmd->uploadBuffer(m_hotbarBgIbo, bgIdxs, sizeof(bgIdxs));
        
        constexpr float HS = 12.0f;
        const float sel[] = { -HS,-HS,0, HS,-HS,0, HS,HS,0, -HS,HS,0 };
        cmd->uploadBuffer(m_hotbarSelVbo, sel, sizeof(sel));
        cmd->uploadBuffer(m_hotbarSelIbo, bgIdxs, sizeof(bgIdxs));
        hudUploaded = true;
    }

    cmd->bindPipeline(m_hudPipeline);
    cmd->setUniform4f("uHalfScreen", winW * 0.5f, winH * 0.5f, 0.0f, 0.0f);

    const float hotbarCentreY = -(winH * 0.5f) + 22.0f;

    if (m_hudVbo && m_hudIbo) {
        cmd->setUniform4f("uAnchor", 0.0f, 0.0f, 0.0f, 0.0f);
        cmd->setUniform4f("uColor", 1.0f, 1.0f, 1.0f, 0.85f);
        cmd->bindVertexBuffer(m_hudVbo, 12);
        cmd->bindIndexBuffer(m_hudIbo, false);
        cmd->drawIndexed(12, 0);
    }
    if (m_hotbarBgVbo && m_hotbarBgIbo) {
        cmd->setUniform4f("uAnchor", 0.0f, hotbarCentreY, 0.0f, 0.0f);
        cmd->setUniform4f("uColor", 0.1f, 0.1f, 0.1f, 0.7f);
        cmd->bindVertexBuffer(m_hotbarBgVbo, 12);
        cmd->bindIndexBuffer(m_hotbarBgIbo, false);
        cmd->drawIndexed(6, 0);
    }
    if (m_hotbarSelVbo && m_hotbarSelIbo) {
        cmd->setUniform4f("uAnchor", -182.0f * 0.5f + 12.0f, hotbarCentreY, 0.0f, 0.0f);
        cmd->setUniform4f("uColor", 1.0f, 1.0f, 1.0f, 0.4f);
        cmd->bindVertexBuffer(m_hotbarSelVbo, 12);
        cmd->bindIndexBuffer(m_hotbarSelIbo, false);
        cmd->drawIndexed(6, 0);
    }
}

void LevelRenderer::loadAtlas(ICommandList* cmd) {
#ifdef _WIN32
    HMODULE hmod = GetModuleHandleW(nullptr);
    HRSRC hresPng = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_BLOCK_ATLAS), RT_RCDATA);
    HRSRC hresJson = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_ATLAS_UV), RT_RCDATA);
    if (!hresPng || !hresJson) return;
    HGLOBAL hgPng = LoadResource(hmod, hresPng);
    HGLOBAL hgJson = LoadResource(hmod, hresJson);
    auto* pngData = static_cast<const uint8_t*>(LockResource(hgPng));
    auto* jsonData = static_cast<const uint8_t*>(LockResource(hgJson));
    DWORD pngSize = SizeofResource(hmod, hresPng);
    DWORD jsonSize = SizeofResource(hmod, hresJson);
    if (pngData && jsonData) m_atlas.load(m_device, cmd, {pngData, pngSize}, {jsonData, jsonSize});
#else
    // Linux: load atlas PNG + JSON from the asset pack on disk
    auto png = mc::AssetManager::instance().readRaw("assets/minecraft/textures/atlas/blocks.png");
    auto js  = mc::AssetManager::instance().readRaw("assets/minecraft/atlases/blocks.json");
    if (!png.empty() && !js.empty()) {
        m_atlas.load(m_device, cmd,
                     {png.data(), png.size()},
                     {js.data(),  js.size()});
    } else {
        // No prebuilt stitched atlas on disk (the stitched blocks.png/.json is a
        // Windows-embedded resource). Fall back to stitching one at runtime from the
        // individual block textures packed in assets.bin (TextureAtlas::loadFromAssetPack).
        MC_LOG_INFO("LevelRenderer: no prebuilt block atlas — stitching from assets.bin");
        m_atlas.load(m_device, cmd, {}, {});
    }
#endif
}

void LevelRenderer::onChunkLoaded(ChunkPos pos) {
    m_renderData.erase(chunkKey(pos));
}

void LevelRenderer::updateCamera(float dtSec) {
    if (!m_camInitialized && m_mc->isInGame()) {
        m_camPos = {(float)m_mc->player().x, (float)m_mc->player().y + 1.62f, (float)m_mc->player().z};
        m_camYaw = m_mc->player().yaw; m_camPitch = m_mc->player().pitch;
        m_camInitialized = true; if (m_window) m_window->captureMouse(true);
    }
    if (!m_window) return;
    int dx = 0, dy = 0; m_window->consumeMouseDelta(dx, dy);
    if (dx || dy) {
        m_camYaw -= (float)dx * 0.15f; m_camPitch += (float)dy * 0.15f;
        m_camPitch = std::fmax(-89.f, std::fmin(89.f, m_camPitch));
    }
    float speed = 20.f; if (m_window->isKeyDown(VK_SHIFT)) speed *= 5.f;
    glm::vec3 fwd = vanillaViewVector(0.0f, m_camYaw);
    glm::vec3 right = glm::normalize(glm::cross(glm::vec3{0.f, 1.f, 0.f}, fwd));
    if (m_window->isKeyDown('W')) m_camPos += fwd * speed * dtSec;
    if (m_window->isKeyDown('S')) m_camPos -= fwd * speed * dtSec;
    if (m_window->isKeyDown('A')) m_camPos -= right * speed * dtSec;
    if (m_window->isKeyDown('D')) m_camPos += right * speed * dtSec;
    if (m_window->isKeyDown(VK_SPACE)) m_camPos.y += speed * dtSec;
    if (m_window->isKeyDown(VK_CONTROL)) m_camPos.y -= speed * dtSec;

    if (m_mc->isInGame() && !m_mc->isConnected()) {
        m_mc->player().x = m_camPos.x;
        m_mc->player().y = m_camPos.y - 1.62f;
        m_mc->player().z = m_camPos.z;
        m_mc->player().yaw = m_camYaw;
        m_mc->player().pitch = m_camPitch;
    }
}

void LevelRenderer::rebuildDirtyChunks() {
    PROFILE_SCOPE("level_renderer_rebuildDirtyChunks");
    for (size_t i = 0; i < m_pendingMeshBuilds.size(); ) {
        if (m_pendingMeshBuilds[i].future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++i;
            continue;
        }

        PendingMeshBuild ready = std::move(m_pendingMeshBuilds[i]);
        if (i + 1 < m_pendingMeshBuilds.size()) {
            m_pendingMeshBuilds[i] = std::move(m_pendingMeshBuilds.back());
        }
        m_pendingMeshBuilds.pop_back();

        std::vector<SectionMesh> meshes = ready.future.get();
        const int64_t key = chunkKey(ready.pos);
        m_meshBuildQueued.erase(key);

        auto chunkIt = m_mc->chunks().find(key);
        if (chunkIt != m_mc->chunks().end()) {
            LevelChunk* chunk = chunkIt->second.get();
            if (chunk && chunk->isLoaded() && !chunk->meshDirty.load()) {
                auto& rd = m_renderData[key];
                for (auto& old : rd.sections) old.destroy(m_device);
                rd.sections = std::move(meshes);
                rd.built = true;
            }
        }
    }

    struct DirtyCandidate {
        int64_t key;
        int distSq;
    };

    std::vector<DirtyCandidate> dirty;
    dirty.reserve(32);
    const int camChunkX = (int)std::floor(m_camPos.x / 16.0f);
    const int camChunkZ = (int)std::floor(m_camPos.z / 16.0f);

    for (auto& [posKey, chunk] : m_mc->chunks()) {
        if (!chunk || !chunk->isLoaded() || !chunk->meshDirty) continue;
        const ChunkPos cp = chunk->pos();
        const int dx = cp.x - camChunkX;
        const int dz = cp.z - camChunkZ;
        dirty.push_back({ posKey, dx * dx + dz * dz });
    }
    if (dirty.empty()) return;

    std::sort(dirty.begin(), dirty.end(), [](const DirtyCandidate& a, const DirtyCandidate& b) {
        return a.distSq < b.distSq;
    });

    // Schedule mesh builds off-thread; ready results are integrated above and
    // section uploads are budgeted during draw.
    constexpr int maxPendingBuilds = 8;
    constexpr int maxSchedulesPerFrame = 2;
    int scheduled = 0;
    for (const DirtyCandidate& cand : dirty) {
        if ((int)m_pendingMeshBuilds.size() >= maxPendingBuilds) break;
        if (m_meshBuildQueued.find(cand.key) != m_meshBuildQueued.end()) continue;

        auto chunkIt = m_mc->chunks().find(cand.key);
        if (chunkIt == m_mc->chunks().end()) continue;
        LevelChunk* chunk = chunkIt->second.get();
        if (!chunk || !chunk->isLoaded() || !chunk->meshDirty) continue;

        ChunkPos cp = chunk->pos();
        LevelChunk* east = m_mc->getChunk({cp.x + 1, cp.z});
        LevelChunk* west = m_mc->getChunk({cp.x - 1, cp.z});
        LevelChunk* south = m_mc->getChunk({cp.x, cp.z + 1});
        LevelChunk* north = m_mc->getChunk({cp.x, cp.z - 1});
        auto center = std::make_shared<LevelChunk>(*chunk);
        std::array<std::shared_ptr<LevelChunk>, 4> neighbors = {
            east ? std::make_shared<LevelChunk>(*east) : nullptr,
            west ? std::make_shared<LevelChunk>(*west) : nullptr,
            south ? std::make_shared<LevelChunk>(*south) : nullptr,
            north ? std::make_shared<LevelChunk>(*north) : nullptr
        };
        chunk->meshDirty = false;
        m_meshBuildQueued.insert(cand.key);
        const TextureAtlas* atlas = m_atlas.isLoaded() ? &m_atlas : nullptr;
        // Build the per-biome colouring snapshot on THIS (main) thread — the biome
        // source's noise router has mutable caches and must not be hit from the worker.
        ensureBiomeData();
        std::shared_ptr<BiomeMeshContext> biomeCtx = makeBiomeContext(cp);
        m_pendingMeshBuilds.push_back({
            cp,
            m_meshPool->enqueue([center = std::move(center), neighbors = std::move(neighbors), atlas, biomeCtx]() {
                const LevelChunk* n[4] = {
                    neighbors[0].get(),
                    neighbors[1].get(),
                    neighbors[2].get(),
                    neighbors[3].get()
                };
                return ChunkMesher::buildChunk(*center, n, atlas, biomeCtx.get());
            })
        });

        if (++scheduled >= maxSchedulesPerFrame) break;
    }
}

void LevelRenderer::ensureBiomeData() {
    if (m_biomeDataLoaded) return;
    m_biomeDataLoaded = true;  // attempt once; on failure leave data null -> fixed tint

    auto loadColormap = [](const std::string& assetPath)
        -> std::shared_ptr<const std::vector<std::int32_t>> {
        std::vector<uint8_t> bytes = mc::AssetManager::instance().readRaw(assetPath);
        if (bytes.empty()) {
            std::ifstream in("assets/client-extract/assets/" + assetPath, std::ios::binary);
            if (in) bytes.assign(std::istreambuf_iterator<char>(in), {});
        }
        if (bytes.empty()) return nullptr;
        int w = 0, h = 0, ch = 0;
        unsigned char* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &ch, 3);
        if (!px || w * h < 65536) { if (px) stbi_image_free(px); return nullptr; }
        auto out = std::make_shared<std::vector<std::int32_t>>((size_t)w * h);
        for (int i = 0; i < w * h; ++i)
            (*out)[i] = std::int32_t(0xFF000000u) | (px[i*3] << 16) | (px[i*3+1] << 8) | px[i*3+2];
        stbi_image_free(px);
        return out;
    };
    m_grassColormap   = loadColormap("minecraft/textures/colormap/grass.png");
    m_foliageColormap = loadColormap("minecraft/textures/colormap/foliage.png");

    std::string dir = m_mc->dataMinecraftDir();
    if (dir.empty()) dir = "26.1.2/data/minecraft";
    try {
        m_biomeRegistry = std::make_unique<mc::biome::BiomeRegistry>(
            mc::biome::BiomeRegistry::loadFromDirectory(dir + "/worldgen/biome"));
    } catch (...) { m_biomeRegistry.reset(); }

    if (!m_grassColormap || !m_foliageColormap || !m_biomeRegistry || m_biomeRegistry->all().empty()) {
        MC_LOG_WARN("Biome colouring: colormap/registry unavailable — using fixed plains-default tint");
        m_biomeRegistry.reset();
    } else {
        MC_LOG_INFO("Biome colouring active ({} biomes)", (int)m_biomeRegistry->all().size());
    }
}

// Build the immutable per-chunk biome snapshot (main thread). A 2D quart-resolution
// grid over the chunk + blend margin at a representative surface Y; the worker reads
// it through biomeAt without touching the (non-thread-safe) biome source.
std::shared_ptr<BiomeMeshContext> LevelRenderer::makeBiomeContext(ChunkPos cp) {
    if (!m_grassColormap || !m_foliageColormap || !m_biomeRegistry) return nullptr;
    auto ctx = std::make_shared<BiomeMeshContext>();
    ctx->grassColormap = m_grassColormap;
    ctx->foliageColormap = m_foliageColormap;

    const int r = ctx->blendRadius;
    const int minQX = (cp.x * 16 - r) >> 2;
    const int minQZ = (cp.z * 16 - r) >> 2;
    const int maxQX = (cp.x * 16 + 15 + r) >> 2;
    const int maxQZ = (cp.z * 16 + 15 + r) >> 2;
    const int w = maxQX - minQX + 1;
    const int d = maxQZ - minQZ + 1;
    const int qy = 64 >> 2;  // representative surface quart-Y (2D snapshot)

    auto grid = std::make_shared<std::vector<const mc::biome::Biome*>>((size_t)w * d, nullptr);
    const mc::biome::BiomeRegistry* reg = m_biomeRegistry.get();
    for (int qz = 0; qz < d; ++qz)
        for (int qx = 0; qx < w; ++qx) {
            const std::string name = m_mc->getNoiseBiomeName(minQX + qx, qy, minQZ + qz);
            (*grid)[(size_t)qz * w + qx] = name.empty() ? nullptr : reg->find(name);
        }

    ctx->biomeAt = [grid, minQX, minQZ, w, d](int x, int, int z) -> const mc::biome::Biome* {
        const int qx = (x >> 2) - minQX;
        const int qz = (z >> 2) - minQZ;
        if (qx < 0 || qx >= w || qz < 0 || qz >= d) return nullptr;
        return (*grid)[(size_t)qz * w + qx];
    };
    return ctx;
}

void LevelRenderer::uploadAndDrawSection(ICommandList* cmd, SectionMesh& mesh) {
    if (mesh.empty()) return;
    if (!mesh.uploaded) {
        if (m_sectionUploadsThisFrame <= 0) return;
        PROFILE_SCOPE("level_renderer_uploadSection");
        if (!mesh.vbo) mesh.vbo = m_device->createBuffer({mesh.vertices.size() * sizeof(ChunkVertex), BufferUsage::Vertex, false});
        if (!mesh.ibo) mesh.ibo = m_device->createBuffer({mesh.indices.size() * sizeof(uint32_t), BufferUsage::Index, false});
        cmd->uploadBuffer(mesh.vbo, mesh.vertices.data(), mesh.vertices.size() * sizeof(ChunkVertex));
        cmd->uploadBuffer(mesh.ibo, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
        mesh.uploaded = true;
        --m_sectionUploadsThisFrame;
    }
    cmd->bindVertexBuffer(mesh.vbo, sizeof(ChunkVertex));
    cmd->bindIndexBuffer(mesh.ibo, false);
    cmd->drawIndexed((uint32_t)mesh.indices.size(), 0);
}

void LevelRenderer::renderLevel(ICommandList* cmd, float partialTick) {
    static bool atlasLoaded = false;
    if (!atlasLoaded) { loadAtlas(cmd); atlasLoaded = true; }

    auto now = Clock::now();
    // Cap dt at 50ms (20 FPS minimum). The previous 100ms cap caused "lagback":
    // when a heavy frame took >100ms, the player only moved 100ms worth while
    // 100ms+ of real time passed, so they appeared to jump backwards relative
    // to their expected position. 50ms is tight enough to prevent huge camera
    // jumps but loose enough that a single 50ms frame doesn't lose movement.
    float dtSec = std::fmin(std::chrono::duration<float>(now - m_lastFrame).count(), 0.05f);
    m_lastFrame = now; updateCamera(dtSec);
    rebuildDirtyChunks();
    m_sectionUploadsThisFrame = 2;

    // Clean up render data for unloaded chunks to prevent memory leaks
    {
        std::vector<int64_t> toRemove;
        for (const auto& [key, rd] : m_renderData) {
            ChunkPos cp{ (int)(key >> 32), (int)(key & 0xFFFFFFFF) };
            if (!m_mc->getChunk(cp)) {
                toRemove.push_back(key);
            }
        }
        for (auto key : toRemove) {
            auto it = m_renderData.find(key);
            if (it != m_renderData.end()) {
                for (auto& mesh : it->second.sections) {
                    mesh.destroy(m_device);
                }
                m_renderData.erase(it);
            }
        }
    }

    if (!m_pipeline) return;

    glm::vec3 f = vanillaViewVector(m_camPitch, m_camYaw);
    glm::mat4 view = glm::lookAt(m_camPos, m_camPos + f, {0,1,0});
    int w = m_window ? m_window->width() : 1280, h = m_window ? m_window->height() : 720;
    glm::mat4 vp = glm::perspective(glm::radians(70.0f), (float)w/(float)h, 0.1f, 512.0f) * view;

    renderSky(cmd, glm::inverse(vp), w, h);
    cmd->bindPipeline(m_pipeline);
    cmd->setUniformMat4("uMVP", glm::value_ptr(vp));
    cmd->setUniform4f("uCamPos", m_camPos.x, m_camPos.y, m_camPos.z, 0.0f);
    cmd->setUniform4f("uFogStart", 200.0f, 0, 0, 0);
    cmd->setUniform4f("uFogEnd", 460.0f, 0, 0, 0);
    cmd->setUniform4f("uFogColor", 0.752f, 0.847f, 1.0f, 1.0f);

    if (m_atlas.isLoaded()) cmd->bindTexture(m_atlas.texture(), 0);

    // Frustum cull at chunk level: skip chunks whose center is behind the camera
    // or outside the far plane. A full frustum test (6 planes) is ideal but even
    // a simple dot-product check against the camera forward vector eliminates
    // ~half the draw calls (everything behind the player).
    const glm::vec3 camForward = vanillaViewVector(m_camPitch, m_camYaw);
    const float farPlane = 512.0f;
    int drawCalls = 0;
    for (auto& [key, rd] : m_renderData) {
        if (!rd.built) continue;
        // Reconstruct chunk world position from key
        const ChunkPos cp{ (int)(key >> 32), (int)(key & 0xFFFFFFFF) };
        const glm::vec3 chunkCenter(cp.x * 16 + 8.0f, m_camPos.y, cp.z * 16 + 8.0f);
        const glm::vec3 toChunk = chunkCenter - m_camPos;
        const float dist = glm::length(toChunk);
        // Skip if beyond far plane
        if (dist > farPlane) continue;
        // Skip if behind camera (dot < 0 means more than 90° off-axis)
        // Use a slightly relaxed threshold (dot < -0.2) so chunks at the edge
        // of the FOV are still drawn.
        if (dist > 16.0f) {  // Don't cull chunks right next to the player
            const float dot = glm::dot(toChunk / dist, camForward);
            if (dot < -0.3f) continue;
        }
        for (auto& m : rd.sections) {
            uploadAndDrawSection(cmd, m);
            ++drawCalls;
        }
    }
    if (m_entityRenderer) {
        m_entityRenderer->renderEntities(cmd, vp);
    }
}

} // namespace mc::render
