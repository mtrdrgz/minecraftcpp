#pragma once
#include "Screen.h"
#include "../components/Button.h"
#include <memory>
#include <vector>

namespace mc::gui::screens {

// Port of the vanilla pause menu's supported local-client actions.
class PauseScreen final : public Screen {
public:
    PauseScreen();

    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mouseX, int mouseY, float pt) override;
    void mouseClicked(double x, double y, int button) override;
    void keyPressed(int key, int scancode, int mods) override;

    void setButtonTextures(render::ITexture* n, render::ITexture* h) { m_btnNormal = n; m_btnHighlight = h; }

private:
    components::Button* addButton(int x, int y, int w, int h, const std::string& text, std::function<void()> onClick);

    std::vector<std::unique_ptr<components::Button>> m_buttons;
    render::ITexture* m_btnNormal = nullptr;
    render::ITexture* m_btnHighlight = nullptr;
};

} // namespace mc::gui::screens
