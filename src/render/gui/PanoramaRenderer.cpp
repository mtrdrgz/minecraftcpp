#include "PanoramaRenderer.h"
#include "../../assets/AssetManager.h"
#include "../../core/Log.h"
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdint>

namespace mc::render {

namespace {
    struct PVertex { float x, y, z, u, v; uint8_t r, g, b, a; };

    // Inward-facing skybox cube ([-1,1]^3), one quad per face, UV 0..1, +Y up.
    // Face order matches m_faces: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z.
    const PVertex CUBE[24] = {
        // +X
        {1,-1,-1, 0,1, 255,255,255,255}, {1,-1, 1, 1,1, 255,255,255,255}, {1, 1, 1, 1,0, 255,255,255,255}, {1, 1,-1, 0,0, 255,255,255,255},
        // -X
        {-1,-1, 1, 0,1, 255,255,255,255}, {-1,-1,-1, 1,1, 255,255,255,255}, {-1, 1,-1, 1,0, 255,255,255,255}, {-1, 1, 1, 0,0, 255,255,255,255},
        // +Y
        {-1, 1,-1, 0,1, 255,255,255,255}, { 1, 1,-1, 1,1, 255,255,255,255}, { 1, 1, 1, 1,0, 255,255,255,255}, {-1, 1, 1, 0,0, 255,255,255,255},
        // -Y
        {-1,-1, 1, 0,1, 255,255,255,255}, { 1,-1, 1, 1,1, 255,255,255,255}, { 1,-1,-1, 1,0, 255,255,255,255}, {-1,-1,-1, 0,0, 255,255,255,255},
        // +Z
        { 1,-1, 1, 0,1, 255,255,255,255}, {-1,-1, 1, 1,1, 255,255,255,255}, {-1, 1, 1, 1,0, 255,255,255,255}, { 1, 1, 1, 0,0, 255,255,255,255},
        // -Z
        {-1,-1,-1, 0,1, 255,255,255,255}, { 1,-1,-1, 1,1, 255,255,255,255}, { 1, 1,-1, 1,0, 255,255,255,255}, {-1, 1,-1, 0,0, 255,255,255,255},
    };
    uint32_t CUBE_IDX[36];

    ITexture* loadFace(IRenderDevice* dev, ICommandList* cmd, const char* path) {
        auto bytes = mc::AssetManager::instance().readRaw(path);
        if (bytes.empty()) return nullptr;
        int w, h, ch;
        stbi_set_flip_vertically_on_load(false);
        uint8_t* p = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &ch, 4);
        if (!p) return nullptr;
        TextureDesc d;
        d.width = (uint32_t)w; d.height = (uint32_t)h; d.format = TextureFormat::RGBA8;
        d.filter = FilterMode::Linear; d.wrap = WrapMode::ClampEdge;
        ITexture* t = dev->createTexture(d);
        cmd->uploadTexture(t, p);
        stbi_image_free(p);
        return t;
    }
}

PanoramaRenderer::PanoramaRenderer(IRenderDevice* device) : m_device(device) {
    PipelineDesc desc;
    desc.vsSource = PANO_VS;
    desc.fsSource = PANO_FS;
    desc.blend    = BlendMode::None;
    desc.depth    = DepthTest::Disabled;
    desc.cull     = CullMode::None; // camera is inside the cube
    desc.layout   = VertexLayout::PositionTexColor;
    m_pipeline = m_device->createPipeline(desc);

    m_vbo = m_device->createBuffer({ sizeof(CUBE), BufferUsage::Vertex, false });
    m_ibo = m_device->createBuffer({ sizeof(CUBE_IDX), BufferUsage::Index, false });
    for (int f = 0; f < 6; ++f) {
        CUBE_IDX[f * 6 + 0] = f * 4 + 0; CUBE_IDX[f * 6 + 1] = f * 4 + 1; CUBE_IDX[f * 6 + 2] = f * 4 + 2;
        CUBE_IDX[f * 6 + 3] = f * 4 + 0; CUBE_IDX[f * 6 + 4] = f * 4 + 2; CUBE_IDX[f * 6 + 5] = f * 4 + 3;
    }
}

PanoramaRenderer::~PanoramaRenderer() {
    for (auto& t : m_faces) if (t) m_device->destroyTexture(t);
    if (m_overlay) m_device->destroyTexture(m_overlay);
    if (m_vbo) m_device->destroyBuffer(m_vbo);
    if (m_ibo) m_device->destroyBuffer(m_ibo);
    if (m_pipeline) m_device->destroyPipeline(m_pipeline);
}

bool PanoramaRenderer::ensureLoaded(ICommandList* cmd) {
    if (m_tried) return m_loaded;
    m_tried = true;

    // Cube face → panorama image mapping.
    //
    // Vanilla Minecraft (CubeMapTexture.java) loads the 6 images in this order
    // into GL cubemap faces 0..5 (+X, -X, +Y, -Y, +Z, -Z):
    //   SUFFIXES = {"_1.png", "_3.png", "_5.png", "_4.png", "_0.png", "_2.png"}
    //
    // Vanilla CubeMap.render() then applies rotationX(PI) to the model-view
    // matrix, which swaps +Y↔-Y and +Z↔-Z (the view direction flips). So the
    // effective mapping the player sees (after rotation) is:
    //   +X (right)  = panorama_1
    //   -X (left)   = panorama_3
    //   +Y (top)    = panorama_4   (was _5 before rotation)
    //   -Y (bottom) = panorama_5   (was _4 before rotation)
    //   +Z (back)   = panorama_2   (was _0 before rotation)
    //   -Z (front)  = panorama_0   (was _2 before rotation)
    //
    // Our C++ cube has NO rotationX(PI) (adding it flipped the images upside
    // down because our UVs are already +Y-up). Instead, we assign textures to
    // match the vanilla POST-rotation appearance directly. This is equivalent
    // to swapping +Y↔-Y and +Z↔-Z face assignments relative to the Java
    // SUFFIXES order.
    static const char* kFaces[6] = {
        "minecraft/textures/gui/title/background/panorama_1.png",  // +X (right)
        "minecraft/textures/gui/title/background/panorama_3.png",  // -X (left)
        "minecraft/textures/gui/title/background/panorama_4.png",  // +Y (top)
        "minecraft/textures/gui/title/background/panorama_5.png",  // -Y (bottom)
        "minecraft/textures/gui/title/background/panorama_2.png",  // +Z (back)
        "minecraft/textures/gui/title/background/panorama_0.png",  // -Z (front)
    };
    bool ok = true;
    for (int i = 0; i < 6; ++i) {
        m_faces[i] = loadFace(m_device, cmd, kFaces[i]);
        if (!m_faces[i]) ok = false;
    }
    m_overlay = loadFace(m_device, cmd, "minecraft/textures/gui/title/background/panorama_overlay.png");

    if (!m_geomUploaded) {
        cmd->uploadBuffer(m_vbo, CUBE, sizeof(CUBE));
        cmd->uploadBuffer(m_ibo, CUBE_IDX, sizeof(CUBE_IDX));
        m_geomUploaded = true;
    }
    m_loaded = ok;
    if (!ok) MC_LOG_WARN("Panorama: some faces missing in assets.bin");
    return m_loaded;
}

void PanoramaRenderer::render(ICommandList* cmd, int width, int height, float dtSeconds) {
    if (!ensureLoaded(cmd) || !m_pipeline) return;

    // Panorama.java: spin += deltaTicks * panoramaSpeed(1) * 0.1  (≈2°/s at 20 TPS).
    m_spin = std::fmod(m_spin + dtSeconds * 20.0f * 0.1f, 360.0f);

    const float aspect = height > 0 ? (float)width / (float)height : 1.0f;
    glm::mat4 proj = glm::perspective(glm::radians(85.0f), aspect, 0.05f, 10.0f);
    // CubeMap.render applies rotationX(PI) for the GL cubemap-sampler convention; our
    // cube is already built +Y-up with explicit UVs, so adding it flipped the panorama
    // upside down. Just spin in yaw.
    glm::mat4 view(1.0f);
    view = glm::rotate(view, glm::radians(m_spin), glm::vec3(0, 1, 0));
    glm::mat4 mvp = proj * view;

    cmd->bindPipeline(m_pipeline);
    cmd->setUniformMat4("uMVP", glm::value_ptr(mvp));
    cmd->bindVertexBuffer(m_vbo, sizeof(PVertex));
    cmd->bindIndexBuffer(m_ibo, true);
    for (int f = 0; f < 6; ++f) {
        if (!m_faces[f]) continue;
        cmd->bindTexture(m_faces[f], 0);
        cmd->drawIndexed(6, f * 6);
    }
}

} // namespace mc::render
