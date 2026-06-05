#include "OptionWidgets.h"
#include <algorithm>

namespace mc::gui::components {

void AbstractWidget::drawBg(render::GuiGraphics& g, bool hover) {
    render::ITexture* tex = hover ? (m_texH ? m_texH : m_texN) : m_texN;
    if (tex) {
        g.blit(tex, m_x, m_y, 0.0f, 0.0f, m_w, m_h, 200, 20);
    } else {
        g.fill(m_x, m_y, m_x + m_w, m_y + m_h, hover ? glm::vec4{ 0.4f, 0.4f, 0.5f, 1.0f } : glm::vec4{ 0.25f, 0.25f, 0.25f, 1.0f });
    }
}

void AbstractWidget::drawLabel(render::GuiGraphics& g, render::Font& font, const std::string& s) {
    const int tw = font.width(s);
    font.drawString(g, s, (float)(m_x + m_w / 2 - tw / 2), (float)(m_y + (m_h - 8) / 2), { 1, 1, 1, 1 });
}

// ── WidgetButton ─────────────────────────────────────────────────────────────
void WidgetButton::render(render::GuiGraphics& g, render::Font& font, int mx, int my) {
    drawBg(g, m_active && hovered(mx, my));
    drawLabel(g, font, m_label);
}
bool WidgetButton::mouseClicked(double x, double y, int button) {
    if (button == 0 && m_active && hovered((int)x, (int)y)) { if (m_onClick) m_onClick(); return true; }
    return false;
}

// ── Slider ───────────────────────────────────────────────────────────────────
void Slider::render(render::GuiGraphics& g, render::Font& font, int mx, int my) {
    drawBg(g, false);
    // Handle (8px) at the value fraction.
    const double frac = m_max > m_min ? std::clamp((m_value - m_min) / (m_max - m_min), 0.0, 1.0) : 0.0;
    const int hx = m_x + (int)(frac * (m_w - 8));
    const bool hover = hovered(mx, my);
    g.fill(hx, m_y, hx + 8, m_y + m_h, hover ? glm::vec4{ 0.75f, 0.75f, 0.75f, 1.0f } : glm::vec4{ 0.55f, 0.55f, 0.55f, 1.0f });
    drawLabel(g, font, m_label + ": " + m_fmt(m_value));
}
bool Slider::mouseClicked(double x, double y, int button) {
    if (button != 0 || !hovered((int)x, (int)y)) return false;
    const double frac = std::clamp((x - m_x) / (double)m_w, 0.0, 1.0);
    m_value = m_min + frac * (m_max - m_min);
    if (m_onChange) m_onChange(m_value);
    return true;
}

// ── CycleButton ──────────────────────────────────────────────────────────────
void CycleButton::render(render::GuiGraphics& g, render::Font& font, int mx, int my) {
    drawBg(g, hovered(mx, my));
    const std::string val = m_choices.empty() ? "" : m_choices[(std::size_t)m_index % m_choices.size()];
    drawLabel(g, font, m_label + ": " + val);
}
bool CycleButton::mouseClicked(double x, double y, int button) {
    if (button != 0 || !hovered((int)x, (int)y) || m_choices.empty()) return false;
    m_index = (m_index + 1) % (int)m_choices.size();
    if (m_onChange) m_onChange(m_index);
    return true;
}

} // namespace mc::gui::components
