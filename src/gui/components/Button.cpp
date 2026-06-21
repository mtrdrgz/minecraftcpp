#include "Button.h"

namespace mc::gui::components {

Button::Button(int x, int y, int w, int h, const std::string& text, std::function<void()> onClick)
    : m_x(x), m_y(y), m_w(w), m_h(h), m_text(text), m_onClick(std::move(onClick)) {
}

void Button::render(render::GuiGraphics& g, render::Font& font, int mouseX, int mouseY) {
    m_hovered = (mouseX >= m_x && mouseX < m_x + m_w && mouseY >= m_y && mouseY < m_y + m_h);

    render::ITexture* tex = m_hovered ? m_texHighlight : m_texNormal;
    if (tex) {
        // Destination can be 204x20, but the vanilla button texture source is
        // 200x20. Sample only the real source rect and stretch it to m_w/m_h;
        // sampling 204 pixels from a 200-pixel texture caused edge artifacts.
        g.blitSized(tex, m_x, m_y, m_w, m_h, 0, 0, 200, 20, 200, 20);
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
