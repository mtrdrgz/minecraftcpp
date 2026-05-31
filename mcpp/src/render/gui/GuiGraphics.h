#pragma once
#include "../../core/Math.h"
#include "../IRenderDevice.h"
#include <vector>

namespace mc::render {

// Port of net.minecraft.client.gui.GuiGraphics
// Simplified version for prototype rendering of UI elements and text.
class GuiGraphics {
public:
    struct GuiVertex {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;
    };

    GuiGraphics(IRenderDevice* device);
    ~GuiGraphics();

    // Primitive drawing
    void fill(int x0, int y0, int x1, int y1, const glm::vec4& color);
    void fillGradient(int x0, int y0, int x1, int y1, const glm::vec4& c0, const glm::vec4& c1);
    
    // Texture drawing
    void blit(ITexture* texture, int x, int y, float u, float v, int w, int h, int texW = 256, int texH = 256);
    void blitAlpha(ITexture* texture, int x, int y, float u, float v, int w, int h, const glm::vec4& color, int texW = 256, int texH = 256);

    // Coordinate system management
    void push() { m_poseStack.push_back(m_poseStack.back()); }
    void pop()  { if (m_poseStack.size() > 1) m_poseStack.pop_back(); }
    void translate(float x, float y, float z = 0.0f);
    void scale(float x, float y, float z = 1.0f);

    // Execution
    void render(ICommandList* cmd, float screenW, float screenH);
    void flush();

private:
    void addQuad(const GuiVertex& v0, const GuiVertex& v1, const GuiVertex& v2, const GuiVertex& v3, ITexture* tex);

    IRenderDevice* m_device;
    IPipeline*     m_pipeline = nullptr;
    IBuffer*       m_vbo = nullptr;
    
    struct Batch {
        ITexture* texture;
        size_t    start;
        size_t    count;
    };

    std::vector<GuiVertex> m_vertices;
    std::vector<Batch>     m_batches;
    std::vector<glm::mat4> m_poseStack;

    ITexture* m_whiteTexture = nullptr;
};

} // namespace mc::render
