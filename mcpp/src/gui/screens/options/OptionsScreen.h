#pragma once
#include "../Screen.h"
#include "../../components/Button.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace mc::gui::screens {

// Port of net.minecraft.client.gui.screens.options.OptionsScreen (title-opened path).
// The 2-column grid of category buttons + a Done button; each category opens its
// sub-screen. The actual OptionInstance widgets (sliders/toggles) inside the
// sub-screens are the next increment — for now each sub-screen is the real titled
// screen with a Done button (navigation works; nothing is invented).
class OptionsScreen : public Screen {
public:
    OptionsScreen();
    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mouseX, int mouseY, float pt) override;
    void mouseClicked(double x, double y, int button) override;

    void setButtonTextures(render::ITexture* n, render::ITexture* h) { m_btnN = n; m_btnH = h; }
    void setBackAction(std::function<void()> f) { m_back = std::move(f); }

private:
    std::vector<std::unique_ptr<components::Button>> m_buttons;
    render::ITexture* m_btnN = nullptr;
    render::ITexture* m_btnH = nullptr;
    std::function<void()> m_back;
};

// A category sub-screen: real title + Done. The option widgets are not ported yet
// (intentionally empty rather than inventing fake controls).
class OptionsSubScreen : public Screen {
public:
    OptionsSubScreen(const std::string& title, render::ITexture* n, render::ITexture* h, std::function<void()> back);
    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mouseX, int mouseY, float pt) override;
    void mouseClicked(double x, double y, int button) override;

private:
    std::vector<std::unique_ptr<components::Button>> m_buttons;
    render::ITexture* m_btnN = nullptr;
    render::ITexture* m_btnH = nullptr;
    std::function<void()> m_back;
};

} // namespace mc::gui::screens
