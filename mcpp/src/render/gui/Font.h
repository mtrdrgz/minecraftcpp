#pragma once
#include "GuiGraphics.h"
#include <string>

namespace mc::render {

// Simple bitmap font renderer using the vanilla ascii.png texture.
class Font {
public:
    Font(IRenderDevice* device, ITexture* texture);
    ~Font() = default;

    // Draw string using provided GuiGraphics batch
    void drawString(GuiGraphics& g, const std::string& text, float x, float y, const glm::vec4& color, bool shadow = true);
    
    // Calculate width of string in pixels
    int width(const std::string& text) const;

private:
    ITexture* m_texture;
    float     m_charWidths[256];
};

} // namespace mc::render
