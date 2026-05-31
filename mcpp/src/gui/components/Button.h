#pragma once
#include "../../render/gui/GuiGraphics.h"
#include "../../render/gui/Font.h"
#include <string>
#include <functional>

namespace mc::gui::components {

// Port of net.minecraft.client.gui.components.Button
class Button {
public:
    Button(int x, int y, int w, int h, const std::string& text, std::function<void()> onClick);
    ~Button() = default;

    void render(render::GuiGraphics& g, render::Font& font, int mouseX, int mouseY);
    bool mouseClicked(double x, double y, int button);

    void setTextures(render::ITexture* normal, render::ITexture* highlight) {
        m_texNormal = normal;
        m_texHighlight = highlight;
    }

private:
    int m_x, m_y, m_w, m_h;
    std::string m_text;
    std::function<void()> m_onClick;
    bool m_hovered = false;

    render::ITexture* m_texNormal = nullptr;
    render::ITexture* m_texHighlight = nullptr;
};

} // namespace mc::gui::components
