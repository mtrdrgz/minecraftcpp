#pragma once
#include "../Screen.h"
#include "../../components/OptionWidgets.h"
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace mc::gui::screens {

// Base for the option category sub-screens (port of OptionsSubScreen + OptionsList).
// Subclasses override addOptions() and build controls with the add* helpers; the base
// lays them out (big = 1 col 310px, small = 2 cols 150px) and adds the Done button.
class OptionsSubScreen : public Screen {
public:
    OptionsSubScreen(const std::string& title, std::function<void()> back);
    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mx, int my, float pt) override;
    void mouseClicked(double x, double y, int button) override;
    void mouseReleased(double x, double y, int button) override;
    void mouseDragged(double x, double y, int button, double dx, double dy) override;
    void setButtonTextures(render::ITexture* n, render::ITexture* h) { m_btnN = n; m_btnH = h; }

protected:
    virtual void addOptions() {}
    void addSlider(const std::string& label, double val, double mn, double mx,
                   std::function<std::string(double)> fmt, std::function<void(double)> onCh, bool big = false);
    void addCycle(const std::string& label, std::vector<std::string> choices, int idx,
                  std::function<void(int)> onCh, bool big = false);
    void addToggle(const std::string& label, bool val, std::function<void(bool)> onCh, bool big = false);
    Minecraft* mc() { return m_minecraft; }

    render::ITexture* m_btnN = nullptr;
    render::ITexture* m_btnH = nullptr;

private:
    struct Pending { std::unique_ptr<components::AbstractWidget> w; bool big; };
    std::vector<Pending> m_pending;
    std::vector<std::unique_ptr<components::AbstractWidget>> m_widgets;
    std::function<void()> m_back;
};

// OptionsScreen: the 2-column category grid + FOV slider + Done.
class OptionsScreen : public Screen {
public:
    OptionsScreen();
    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mx, int my, float pt) override;
    void mouseClicked(double x, double y, int button) override;
    void mouseReleased(double x, double y, int button) override;
    void mouseDragged(double x, double y, int button, double dx, double dy) override;
    void setButtonTextures(render::ITexture* n, render::ITexture* h) { m_btnN = n; m_btnH = h; }
    void setBackAction(std::function<void()> f) { m_back = std::move(f); }

private:
    std::vector<std::unique_ptr<components::AbstractWidget>> m_widgets;
    render::ITexture* m_btnN = nullptr;
    render::ITexture* m_btnH = nullptr;
    std::function<void()> m_back;
};

// Concrete category screens (real options ported from the Java).
class SoundOptionsScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};
class VideoSettingsScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};
class ControlsScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

} // namespace mc::gui::screens
