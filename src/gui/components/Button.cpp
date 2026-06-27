#include "Button.h"

namespace mc::gui::components {

Button::Button(int x, int y, int w, int h, const std::string& text, std::function<void()> onClick)
    : m_x(x), m_y(y), m_w(w), m_h(h), m_text(text), m_onClick(std::move(onClick)) {
}

void Button::render(render::GuiGraphics& g, render::Font& font, int mouseX, int mouseY) {
    m_hovered = (mouseX >= m_x && mouseX < m_x + m_w && mouseY >= m_y && mouseY < m_y + m_h);

    render::ITexture* tex = m_hovered ? m_texHighlight : m_texNormal;
    if (tex) {
        // 1:1 with vanilla AbstractButton.extractDefaultSprite: the button.png
        // sprite is 200x20 with a 3px nine-slice border (button.png.mcmeta).
        // blitNineSlice keeps the 3px bevel crisp at any widget size while
        // stretching the center — the simple stretch used before distorted the
        // corners and the highlighted variant showed edge artifacts.
        g.blitNineSlice(tex, m_x, m_y, m_w, m_h, 200, 20, 3, 3, 3, 3);
    } else {
        // Fallback
        g.fill(m_x, m_y, m_x + m_w, m_y + m_h, m_hovered ? glm::vec4{0.7f, 0.7f, 1.0f, 1.0f} : glm::vec4{0.4f, 0.4f, 0.4f, 1.0f});
    }

    int textW = font.width(m_text);
    font.drawString(g, m_text, (float)(m_x + m_w / 2 - textW / 2), (float)(m_y + (m_h - 8) / 2), {1, 1, 1, 1});
}

bool Button::mouseClicked(double x, double y, int button) {
    std::function<void()> action = clickAction(x, y, button);
    if (action) {
        action();
        return true;
    }
    return false;
}

std::function<void()> Button::clickAction(double x, double y, int button) const {
    if (button == 0 && x >= m_x && x < m_x + m_w && y >= m_y && y < m_y + m_h) {
        return m_onClick;
    }
    return {};
}

} // namespace mc::gui::components
