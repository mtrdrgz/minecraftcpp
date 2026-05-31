#include "GuiGraphics.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace mc::render {

static constexpr const char* GUI_VS = R"(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

out vec2 vTexCoord;
out vec4 vColor;

uniform vec4 uScreenSize;

void main() {
    float x = (aPos.x / (uScreenSize.x * 0.5)) - 1.0;
    float y = 1.0 - (aPos.y / (uScreenSize.y * 0.5));
    gl_Position = vec4(x, y, aPos.z, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)";
static constexpr const char* GUI_FS = R"(
#version 460 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 fragColor;

layout(binding = 0) uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    fragColor = texColor * vColor;
    if (fragColor.a < 0.01) discard;
}
)";

GuiGraphics::GuiGraphics(IRenderDevice* device) : m_device(device) {
    m_poseStack.push_back(glm::mat4(1.0f));
    
    PipelineDesc desc;
    desc.vsSource = GUI_VS;
    desc.fsSource = GUI_FS;
    desc.blend = BlendMode::Alpha;
    desc.depth = DepthTest::Disabled;
    desc.cull = CullMode::None;
    desc.layout = VertexLayout::Gui;
    m_pipeline = m_device->createPipeline(desc);

    m_vbo = m_device->createBuffer({sizeof(GuiVertex) * 8192, BufferUsage::Vertex, true});
    
    TextureDesc texDesc;
    texDesc.width = 1;
    texDesc.height = 1;
    texDesc.format = TextureFormat::RGBA8;
    texDesc.filter = FilterMode::Nearest;
    m_whiteTexture = m_device->createTexture(texDesc);
}

GuiGraphics::~GuiGraphics() {
    if (m_whiteTexture) m_device->destroyTexture(m_whiteTexture);
    if (m_vbo) m_device->destroyBuffer(m_vbo);
    if (m_pipeline) m_device->destroyPipeline(m_pipeline);
}

void GuiGraphics::fill(int x0, int y0, int x1, int y1, const glm::vec4& color) {
    fillGradient(x0, y0, x1, y1, color, color);
}

void GuiGraphics::fillGradient(int x0, int y0, int x1, int y1, const glm::vec4& c0, const glm::vec4& c1) {
    GuiVertex v0{{(float)x0, (float)y0, 0.0f}, {0,0}, c0};
    GuiVertex v1{{(float)x1, (float)y0, 0.0f}, {1,0}, c0};
    GuiVertex v2{{(float)x1, (float)y1, 0.0f}, {1,1}, c1};
    GuiVertex v3{{(float)x0, (float)y1, 0.0f}, {0,1}, c1};
    addQuad(v0, v1, v2, v3, m_whiteTexture);
}

void GuiGraphics::blit(ITexture* tex, int x, int y, float u, float v, int w, int h, int tw, int th) {
    blitAlpha(tex, x, y, u, v, w, h, {1,1,1,1}, tw, th);
}

void GuiGraphics::blitAlpha(ITexture* tex, int x, int y, float u, float v, int w, int h, const glm::vec4& col, int tw, int th) {
    float u0_f = u / (float)tw;
    float v0_f = v / (float)th;
    float u1_f = (u + w) / (float)tw;
    float v1_f = (v + h) / (float)th;

    GuiVertex v0_v{{(float)x,   (float)y,   0.0f}, {u0_f, v0_f}, col};
    GuiVertex v1_v{{(float)x+w, (float)y,   0.0f}, {u1_f, v0_f}, col};
    GuiVertex v2_v{{(float)x+w, (float)y+h, 0.0f}, {u1_f, v1_f}, col};
    GuiVertex v3_v{{(float)x,   (float)y+h, 0.0f}, {u0_f, v1_f}, col};
    addQuad(v0_v, v1_v, v2_v, v3_v, tex);
}

void GuiGraphics::translate(float x, float y, float z) {
    m_poseStack.back() = glm::translate(m_poseStack.back(), {x, y, z});
}

void GuiGraphics::scale(float x, float y, float z) {
    m_poseStack.back() = glm::scale(m_poseStack.back(), {x, y, z});
}

void GuiGraphics::addQuad(const GuiVertex& v0, const GuiVertex& v1, const GuiVertex& v2, const GuiVertex& v3, ITexture* tex) {
    if (!m_batches.empty() && m_batches.back().texture == tex) {
        m_batches.back().count += 6;
    } else {
        m_batches.push_back({tex, m_vertices.size(), 6});
    }

    const glm::mat4& m = m_poseStack.back();
    auto xform = [&](const GuiVertex& v) {
        GuiVertex out = v;
        glm::vec4 p = m * glm::vec4(v.pos, 1.0f);
        out.pos = {p.x, p.y, p.z};
        return out;
    };

    GuiVertex tv0 = xform(v0);
    GuiVertex tv1 = xform(v1);
    GuiVertex tv2 = xform(v2);
    GuiVertex tv3 = xform(v3);

    m_vertices.push_back(tv0);
    m_vertices.push_back(tv1);
    m_vertices.push_back(tv2);
    m_vertices.push_back(tv0);
    m_vertices.push_back(tv2);
    m_vertices.push_back(tv3);
}

void GuiGraphics::flush() {
    m_vertices.clear();
    m_batches.clear();
}

void GuiGraphics::render(ICommandList* cmd, float sw, float sh) {
    if (m_vertices.empty()) return;

    static bool whiteUploaded = false;
    if (!whiteUploaded) {
        uint32_t white = 0xFFFFFFFF;
        cmd->uploadTexture(m_whiteTexture, &white);
        whiteUploaded = true;
    }

    cmd->uploadBuffer(m_vbo, m_vertices.data(), m_vertices.size() * sizeof(GuiVertex), 0);
    cmd->bindPipeline(m_pipeline);
    cmd->setUniform4f("uScreenSize", sw, sh, 0, 0);

    cmd->bindVertexBuffer(m_vbo, sizeof(GuiVertex));

    for (const auto& b : m_batches) {
        cmd->bindTexture(b.texture, 0);
        cmd->draw((uint32_t)b.count, (uint32_t)b.start);
    }

    flush();
}

} // namespace mc::render
