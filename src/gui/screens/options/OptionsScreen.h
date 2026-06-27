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
// lays them out (big = 1 col 310px, small = 2 cols 150px), handles scrolling, and
// adds the Done button.
//
// Layout (1:1 with OptionsSubScreen.java + HeaderAndFooterLayout):
//   - Header (33px): title text centered
//   - Header separator (2px): header_separator.png tiled across list width
//   - Contents (scrollable): OptionsList — big rows = 310px, small rows = 150px×2
//   - Footer separator (2px): footer_separator.png tiled across list width
//   - Footer (33px): Done button centered
// When the list content exceeds the available height, a scrollbar appears on the
// right (scroller.png + scroller_background.png). Mouse wheel scrolls the list.
class OptionsSubScreen : public Screen {
public:
    OptionsSubScreen(const std::string& title, std::function<void()> back);
    void init(Minecraft* mc, int w, int h) override;
    void render(render::GuiGraphics& g, int mx, int my, float pt) override;
    void mouseClicked(double x, double y, int button) override;
    void mouseReleased(double x, double y, int button) override;
    void mouseDragged(double x, double y, int button, double dx, double dy) override;
    void mouseScrolled(double x, double y, double dx, double dy) override;
    void setButtonTextures(render::ITexture* n, render::ITexture* h) { m_btnN = n; m_btnH = h; }
    void setSliderTextures(render::ITexture* track, render::ITexture* handle, render::ITexture* handleHl) {
        m_sliderTrack = track; m_sliderHandle = handle; m_sliderHandleHl = handleHl;
    }
    // Separator + scrollbar textures. Loaded by Minecraft.cpp and passed to
    // every OptionsSubScreen so the list can draw header/footer separators +
    // the scrollbar. When null, the list draws grey lines as fallback.
    void setListTextures(render::ITexture* headerSep, render::ITexture* footerSep,
                         render::ITexture* scroller, render::ITexture* scrollerBg,
                         render::ITexture* listBg) {
        m_headerSep = headerSep; m_footerSep = footerSep;
        m_scroller = scroller; m_scrollerBg = scrollerBg; m_listBg = listBg;
    }

protected:
    virtual void addOptions() {}
    void addSlider(const std::string& label, double val, double mn, double mx,
                   std::function<std::string(double)> fmt, std::function<void(double)> onCh, bool big = false);
    void addCycle(const std::string& label, std::vector<std::string> choices, int idx,
                  std::function<void(int)> onCh, bool big = false);
    void addToggle(const std::string& label, bool val, std::function<void(bool)> onCh, bool big = false);
    // Add a section header (like "Display" / "Quality" / "Preferences" in
    // VideoSettingsScreen). 1:1 with OptionsList.addHeader.
    void addHeader(const std::string& text);
    Minecraft* mc() { return m_minecraft; }

    render::ITexture* m_btnN = nullptr;
    render::ITexture* m_btnH = nullptr;
    render::ITexture* m_sliderTrack = nullptr;
    render::ITexture* m_sliderHandle = nullptr;
    render::ITexture* m_sliderHandleHl = nullptr;

private:
    struct Pending { std::unique_ptr<components::AbstractWidget> w; bool big; };
    struct Row {
        std::unique_ptr<components::AbstractWidget> left;
        std::unique_ptr<components::AbstractWidget> right;  // null for big/header
        bool isHeader = false;
        std::string headerText;
        int height = 25;  // DEFAULT_ITEM_HEIGHT from OptionsList.java
    };
    std::vector<Pending> m_pending;
    std::vector<Row> m_rows;
    std::unique_ptr<components::AbstractWidget> m_doneBtn;
    std::function<void()> m_back;

    // Scrolling state
    double m_scrollAmount = 0.0;
    bool m_scrollingBar = false;

    // List geometry (computed in init)
    int m_listX = 0, m_listY = 0, m_listW = 0, m_listH = 0;
    int m_headerHeight = 33, m_footerHeight = 33;

    // List textures
    render::ITexture* m_headerSep = nullptr;
    render::ITexture* m_footerSep = nullptr;
    render::ITexture* m_scroller = nullptr;
    render::ITexture* m_scrollerBg = nullptr;
    render::ITexture* m_listBg = nullptr;

    int maxScroll() const;
    int contentHeight() const;
    void clampScroll();
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
    void setSliderTextures(render::ITexture* track, render::ITexture* handle, render::ITexture* handleHl) {
        m_sliderTrack = track; m_sliderHandle = handle; m_sliderHandleHl = handleHl;
    }
    void setBackAction(std::function<void()> f) { m_back = std::move(f); }

private:
    std::vector<std::unique_ptr<components::AbstractWidget>> m_widgets;
    render::ITexture* m_btnN = nullptr;
    render::ITexture* m_btnH = nullptr;
    render::ITexture* m_sliderTrack = nullptr;
    render::ITexture* m_sliderHandle = nullptr;
    render::ITexture* m_sliderHandleHl = nullptr;
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

// AccessibilityOptionsScreen: port of AccessibilityOptionsScreen.options().
class AccessibilityOptionsScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

// LanguageSelectScreen: simplified port (cycle button, not scrolling list).
class LanguageSelectScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

// SkinCustomizationScreen: port of SkinCustomizationScreen.addOptions().
// Vanilla toggles PlayerModelPart values (cape, jacket, left_sleeve,
// right_sleeve, left_pants_leg, right_pants_leg, hat) + main hand cycle.
class SkinCustomizationScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

// ChatOptionsScreen: port of ChatOptionsScreen.options(). 18 chat-related
// controls (visibility, colors, links, opacity, scale, spacing, delay, width,
// height, narrator, suggestions, hide names, reduced debug, secure chat, save
// drafts).
class ChatOptionsScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

// ResourcePacksScreen: simplified port. Vanilla has a two-pane list of
// available/selected resource packs. We show a placeholder message + Done.
class ResourcePacksScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

// TelemetryInfoScreen: port of TelemetryInfoScreen. Vanilla shows telemetry
// toggle + info text + links. We show the toggle + Done.
class TelemetryInfoScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

// CreditsAndAttributionScreen: port of CreditsAndAttributionScreen. Three
// buttons (Credits, Attribution, Licenses) + Done. The Credits button in
// vanilla opens WinScreen (the end-game credits scroll); we make it a no-op.
class CreditsAndAttributionScreen final : public OptionsSubScreen {
public: using OptionsSubScreen::OptionsSubScreen;
protected: void addOptions() override;
};

} // namespace mc::gui::screens
