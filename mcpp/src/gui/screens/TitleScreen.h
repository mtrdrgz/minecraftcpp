#pragma once
#include "Screen.h"
#include "../components/Button.h"
#include <vector>
#include <memory>

namespace mc::gui::screens {

// Port of net.minecraft.client.gui.screens.TitleScreen
class TitleScreen : public Screen {
public:
    TitleScreen();
    ~TitleScreen() override;

    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mouseX, int mouseY, float pt) override;
    void mouseClicked(double x, double y, int button) override;

    void setLogoTexture(render::ITexture* t) { m_logoTex = t; }
    void setButtonTextures(render::ITexture* n, render::ITexture* h) { m_btnNormal = n; m_btnHighlight = h; }
    void setDirtTexture(render::ITexture* t) { m_dirtTex = t; }
    void setEditionTexture(render::ITexture* t) { m_editionTex = t; }

private:
    components::Button* addButton(int x, int y, int w, int h, const std::string& text, std::function<void()> onClick);
    void renderDirtBackground(render::GuiGraphics& g);

    std::vector<std::unique_ptr<components::Button>> m_buttons;
    render::ITexture* m_logoTex = nullptr;
    render::ITexture* m_editionTex = nullptr;
    render::ITexture* m_dirtTex = nullptr;
    render::ITexture* m_btnNormal = nullptr;
    render::ITexture* m_btnHighlight = nullptr;
};

} // namespace mc::gui::screens
