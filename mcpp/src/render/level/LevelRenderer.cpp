#include "LevelRenderer.h"
#include "../../world/level/block/Blocks.h"
#include "../../assets/resource_ids.h"
#include "../../core/Log.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace mc::render {

LevelRenderer::LevelRenderer(IRenderDevice* device, Minecraft* mc, Window* window)
    : m_device(device), m_mc(mc), m_window(window)
{
    setupChunkPipeline();
    setupSkyPipeline();
    setupHudPipeline();
    m_entityRenderer = std::make_unique<EntityRenderDispatcher>(device);
    // Atlas load needs a command list, so we defer it to the first render pass
}

LevelRenderer::~LevelRenderer() {
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
        m_camYaw += (float)dx * 0.15f; m_camPitch -= (float)dy * 0.15f;
        m_camPitch = std::fmax(-89.f, std::fmin(89.f, m_camPitch));
    }
    float speed = 20.f; if (m_window->isKeyDown(VK_SHIFT)) speed *= 5.f;
    float yawRad = glm::radians(m_camYaw);
    glm::vec3 fwd{ sinf(yawRad), 0.f, cosf(yawRad) }, right{ cosf(yawRad), 0.f, -sinf(yawRad) };
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
    constexpr int MAX_REBUILDS = 4; int rebuilt = 0;
    for (auto& [posKey, chunk] : m_mc->chunks()) {
        if (!chunk || !chunk->isLoaded() || !chunk->meshDirty) continue;
        MC_LOG_INFO("Rebuilding chunk at key {}", posKey);
        ChunkPos cp = chunk->pos();
        const LevelChunk* n[4] = { m_mc->getChunk({cp.x+1, cp.z}), m_mc->getChunk({cp.x-1, cp.z}), m_mc->getChunk({cp.x, cp.z+1}), m_mc->getChunk({cp.x, cp.z-1}) };
        auto meshes = ChunkMesher::buildChunk(*chunk, n, m_atlas.isLoaded() ? &m_atlas : nullptr);
        chunk->meshDirty = false;
        auto& rd = m_renderData[posKey];
        for (auto& old : rd.sections) old.destroy(m_device);
        rd.sections = std::move(meshes); rd.built = true;
        MC_LOG_INFO("Chunk rebuilt, {} sections created", rd.sections.size());
        if (++rebuilt >= MAX_REBUILDS) break;
    }
}

void LevelRenderer::uploadAndDrawSection(ICommandList* cmd, SectionMesh& mesh) {
    if (mesh.empty()) return;
    if (!mesh.uploaded) {
        if (!mesh.vbo) mesh.vbo = m_device->createBuffer({mesh.vertices.size() * sizeof(ChunkVertex), BufferUsage::Vertex, false});
        if (!mesh.ibo) mesh.ibo = m_device->createBuffer({mesh.indices.size() * sizeof(uint32_t), BufferUsage::Index, false});
        cmd->uploadBuffer(mesh.vbo, mesh.vertices.data(), mesh.vertices.size() * sizeof(ChunkVertex));
        cmd->uploadBuffer(mesh.ibo, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
        mesh.uploaded = true;
    }
    cmd->bindVertexBuffer(mesh.vbo, sizeof(ChunkVertex));
    cmd->bindIndexBuffer(mesh.ibo, false);
    cmd->drawIndexed((uint32_t)mesh.indices.size(), 0);
}

void LevelRenderer::renderLevel(ICommandList* cmd, float partialTick) {
    static bool atlasLoaded = false;
    if (!atlasLoaded) { loadAtlas(cmd); atlasLoaded = true; }

    auto now = Clock::now();
    float dtSec = std::fmin(std::chrono::duration<float>(now - m_lastFrame).count(), 0.1f);
    m_lastFrame = now; updateCamera(dtSec); rebuildDirtyChunks();


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

    float pR = glm::radians(m_camPitch), yR = glm::radians(m_camYaw);
    glm::vec3 f{ cosf(pR)*sinf(yR), sinf(pR), cosf(pR)*cosf(yR) };
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

    for (auto& [key, rd] : m_renderData) {
        if (!rd.built) continue;
        for (auto& m : rd.sections) uploadAndDrawSection(cmd, m);
    }
    if (m_entityRenderer) {
        m_entityRenderer->renderEntities(cmd, vp);
    }
}

} // namespace mc::render
