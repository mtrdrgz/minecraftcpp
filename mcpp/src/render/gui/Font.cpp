#include "Font.h"
#include <glm/glm.hpp>

namespace mc::render {

Font::Font(IRenderDevice* device, ITexture* texture) : m_texture(texture) {
    // Standard Minecraft ASCII widths (default 6px + 2px padding = 8px grid)
    for (int i = 0; i < 256; ++i) {
        m_charWidths[i] = 6.0f; 
    }
    
    // Custom widths for a few common chars (matching vanilla-ish look)
    m_charWidths[' '] = 4.0f;
    m_charWidths['i'] = 2.0f;
    m_charWidths['l'] = 3.0f;
    m_charWidths['t'] = 4.0f;
    m_charWidths['f'] = 5.0f;
    m_charWidths['I'] = 4.0f;
    m_charWidths['!'] = 2.0f;
    m_charWidths['.'] = 2.0f;
    m_charWidths[','] = 2.0f;
}

void Font::drawString(GuiGraphics& g, const std::string& text, float x, float y, const glm::vec4& color, bool shadow) {
    auto draw = [&](float ox, float oy, const glm::vec4& col) {
        float cx = x + ox;
        for (uint8_t c : text) {
            // Basic ASCII grid [0..255] in 16x16 layout
            int row = c / 16;
            int col_idx = c % 16;
            
            // Map 8x8 character cell from 128x128 texture
            g.blitAlpha(m_texture, (int)cx, (int)(y + oy), 
                       (float)col_idx * 8.0f, (float)row * 8.0f, 
                       8, 8, col, 128, 128);
                       
            cx += m_charWidths[c];
        }
    };

    if (shadow) {
        draw(1.0f, 1.0f, {color.r * 0.25f, color.g * 0.25f, color.b * 0.25f, color.a});
    }
    draw(0.0f, 0.0f, color);
}

int Font::width(const std::string& text) const {
    float w = 0.0f;
    for (uint8_t c : text) {
        w += m_charWidths[c];
    }
    return (int)w;
}

} // namespace mc::render
