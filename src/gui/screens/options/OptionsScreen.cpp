#include "OptionsScreen.h"
#include "../../../client/Minecraft.h"
#include "../../../client/Options.h"
#include <string>
#include <cmath>

namespace mc::gui::screens {

using components::AbstractWidget;
using components::WidgetButton;
using components::Slider;
using components::CycleButton;

namespace {
    void drawCentered(render::GuiGraphics& g, render::Font* font, const std::string& s, int cx, int y) {
        if (font) font->drawString(g, s, (float)(cx - font->width(s) / 2), (float)y, { 1, 1, 1, 1 });
    }
    std::string pct100(double v) { return v <= 0.0 ? std::string("OFF") : std::to_string((int)(v * 100 + 0.5)) + "%"; }
}

// ── OptionsSubScreen (base) ──────────────────────────────────────────────────
OptionsSubScreen::OptionsSubScreen(const std::string& title, std::function<void()> back)
    : Screen(title), m_back(std::move(back)) {}

void OptionsSubScreen::addSlider(const std::string& label, double val, double mn, double mx,
                                 std::function<std::string(double)> fmt, std::function<void(double)> onCh, bool big) {
    m_pending.push_back({ std::make_unique<Slider>(0, 0, 150, 20, label, val, mn, mx, std::move(fmt), std::move(onCh)), big });
}
void OptionsSubScreen::addCycle(const std::string& label, std::vector<std::string> choices, int idx,
                                std::function<void(int)> onCh, bool big) {
    m_pending.push_back({ std::make_unique<CycleButton>(0, 0, 150, 20, label, std::move(choices), idx, std::move(onCh)), big });
}
void OptionsSubScreen::addToggle(const std::string& label, bool val, std::function<void(bool)> onCh, bool big) {
    addCycle(label, { "OFF", "ON" }, val ? 1 : 0, [onCh = std::move(onCh)](int i) { onCh(i == 1); }, big);
}

void OptionsSubScreen::init(Minecraft* mcp, int w, int h) {
    Screen::init(mcp, w, h);
    m_pending.clear();
    m_widgets.clear();
    addOptions();

    const int x0 = w / 2 - 155, x1 = w / 2 + 5;
    int y = 40, smallCol = 0;
    for (auto& p : m_pending) {
        if (p.big) {
            if (smallCol != 0) { y += 24; smallCol = 0; }
            p.w->setPos(w / 2 - 155, y); p.w->setSize(310, 20);
            y += 24;
        } else {
            p.w->setPos(smallCol == 0 ? x0 : x1, y); p.w->setSize(150, 20);
            if (smallCol == 1) { y += 24; smallCol = 0; } else { smallCol = 1; }
        }
        p.w->setTextures(m_btnN, m_btnH);
        p.w->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
        m_widgets.push_back(std::move(p.w));
    }
    m_pending.clear();

    auto done = std::make_unique<WidgetButton>(w / 2 - 100, h - 27, 200, 20, "Done", [this]() { if (m_back) m_back(); });
    done->setTextures(m_btnN, m_btnH);
    m_widgets.push_back(std::move(done));
}

void OptionsSubScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.6f });
    drawCentered(g, m_minecraft->font(), m_title, m_width / 2, 15);
    render::Font* font = m_minecraft->font();
    for (auto& widget : m_widgets) widget->render(g, *font, mx, my);
}

void OptionsSubScreen::mouseClicked(double x, double y, int button) {
    std::function<void()> action;
    for (auto& widget : m_widgets) {
        action = widget->clickAction(x, y, button);
        if (action) break;
        if (widget->mouseClicked(x, y, button)) return;
    }
    if (action) action();
}

void OptionsSubScreen::mouseReleased(double x, double y, int button) {
    for (auto& widget : m_widgets) if (widget->mouseReleased(x, y, button)) break;
}

void OptionsSubScreen::mouseDragged(double x, double y, int button, double dx, double dy) {
    for (auto& widget : m_widgets) if (widget->mouseDragged(x, y, button, dx, dy)) break;
}

// ── OptionsScreen (category grid + FOV) ──────────────────────────────────────
OptionsScreen::OptionsScreen() : Screen("Options") {}

void OptionsScreen::init(Minecraft* mcp, int w, int h) {
    Screen::init(mcp, w, h);
    m_widgets.clear();

    auto add = [&](std::unique_ptr<AbstractWidget> wgt) {
        wgt->setTextures(m_btnN, m_btnH);
        wgt->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
        m_widgets.push_back(std::move(wgt));
    };
    Minecraft* mc = m_minecraft;
    auto openSub = [mc, this](std::unique_ptr<gui::Screen> s) {
        if (auto* sub = dynamic_cast<OptionsSubScreen*>(s.get())) {
            sub->setButtonTextures(m_btnN, m_btnH);
            sub->setSliderTextures(m_sliderTrack, m_sliderHandle, m_sliderHandleHl);
        }
        mc->setScreen(std::move(s));
    };
    auto back = [mc]() { mc->openOptionsScreen(); };

    // Subheader: FOV slider (OptionsScreen.java puts FOV here).
    GameOptions& o = mc->options();
    {
        auto fov = std::make_unique<Slider>(w / 2 - 100, h / 6, 200, 20, "FOV", o.fov, 30.0, 110.0,
            [](double v) { int f = (int)(v + 0.5); return f >= 110 ? std::string("Quake Pro") : std::to_string(f); },
            [&o](double v) { o.fov = v; });
        add(std::move(fov));
    }

    // 2-column category grid.
    // kind: 0=generic,1=sound,2=video,3=controls,4=language,5=accessibility,
    //       6=skin,7=chat,8=resourcepacks,9=telemetry,10=credits
    struct Cat { const char* label; int kind; };
    static const Cat CATS[] = {
        { "Skin Customization...", 6 }, { "Music & Sounds...", 1 }, { "Video Settings...", 2 },
        { "Controls...", 3 }, { "Language...", 4 }, { "Chat Settings...", 7 },
        { "Resource Packs...", 8 }, { "Accessibility Settings", 5 }, { "Telemetry Data...", 9 },
        { "Credits & Attribution...", 10 },
    };
    const int x0 = w / 2 - 155, x1 = w / 2 + 5, top = h / 6 + 24;
    for (int i = 0; i < (int)(sizeof(CATS) / sizeof(CATS[0])); ++i) {
        const int col = i % 2, row = i / 2;
        const std::string label = CATS[i].label;
        std::string title = label;
        if (auto p = title.find("..."); p != std::string::npos) title = title.substr(0, p);
        const int kind = CATS[i].kind;
        add(std::make_unique<WidgetButton>(col == 0 ? x0 : x1, top + row * 24, 150, 20, label,
            [openSub, back, title, kind]() {
                std::unique_ptr<gui::Screen> s;
                switch (kind) {
                    case 1:  s = std::make_unique<SoundOptionsScreen>(title, back); break;
                    case 2:  s = std::make_unique<VideoSettingsScreen>(title, back); break;
                    case 3:  s = std::make_unique<ControlsScreen>(title, back); break;
                    case 4:  s = std::make_unique<LanguageSelectScreen>(title, back); break;
                    case 5:  s = std::make_unique<AccessibilityOptionsScreen>(title, back); break;
                    case 6:  s = std::make_unique<SkinCustomizationScreen>(title, back); break;
                    case 7:  s = std::make_unique<ChatOptionsScreen>(title, back); break;
                    case 8:  s = std::make_unique<ResourcePacksScreen>(title, back); break;
                    case 9:  s = std::make_unique<TelemetryInfoScreen>(title, back); break;
                    case 10: s = std::make_unique<CreditsAndAttributionScreen>(title, back); break;
                    default: s = std::make_unique<OptionsSubScreen>(title, back); break;
                }
                openSub(std::move(s));
            }));
    }

    add(std::make_unique<WidgetButton>(w / 2 - 100, h - 27, 200, 20, "Done", [this]() { if (m_back) m_back(); }));
}

void OptionsScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.6f });
    drawCentered(g, m_minecraft->font(), "Options", m_width / 2, 15);
    render::Font* font = m_minecraft->font();
    for (auto& widget : m_widgets) widget->render(g, *font, mx, my);
}

void OptionsScreen::mouseClicked(double x, double y, int button) {
    std::function<void()> action;
    for (auto& widget : m_widgets) {
        action = widget->clickAction(x, y, button);
        if (action) break;
        if (widget->mouseClicked(x, y, button)) return;
    }
    if (action) action();
}

void OptionsScreen::mouseReleased(double x, double y, int button) {
    for (auto& widget : m_widgets) if (widget->mouseReleased(x, y, button)) break;
}

void OptionsScreen::mouseDragged(double x, double y, int button, double dx, double dy) {
    for (auto& widget : m_widgets) if (widget->mouseDragged(x, y, button, dx, dy)) break;
}

// ── Concrete category screens (real options from the Java) ───────────────────
void SoundOptionsScreen::addOptions() {
    GameOptions& o = mc()->options();
    // SoundOptionsScreen.java: master (big), then every other SoundSource (small),
    // then subtitles + directional audio.
    static const char* NAMES[] = { "Master Volume", "Music", "Jukebox/Note Blocks", "Weather",
        "Blocks", "Hostile Creatures", "Friendly Creatures", "Players", "Ambient/Environment", "Voice/Speech" };
    addSlider(NAMES[0], o.volume[0], 0.0, 1.0, pct100, [&o](double v) { o.volume[0] = (float)v; }, true);
    for (int i = 1; i < (int)audio::SoundSource::COUNT; ++i)
        addSlider(NAMES[i], o.volume[i], 0.0, 1.0, pct100, [&o, i](double v) { o.volume[i] = (float)v; });
    addToggle("Show Subtitles", o.showSubtitles, [&o](bool b) { o.showSubtitles = b; });
    addToggle("Directional Audio", o.directionalAudio, [&o](bool b) { o.directionalAudio = b; });
}

void VideoSettingsScreen::addOptions() {
    GameOptions& o = mc()->options();
    addCycle("Graphics", { "Fast", "Fancy", "Fabulous!" }, o.graphics, [&o](int i) { o.graphics = i; }, true);
    addSlider("Render Distance", (double)o.renderDistance, 2.0, 32.0,
              [](double v) { return std::to_string((int)(v + 0.5)) + " chunks"; },
              [&o](double v) { o.renderDistance = (int)(v + 0.5); });
    addCycle("GUI Scale", { "Auto", "1", "2", "3", "4" }, o.guiScale, [&o](int i) { o.guiScale = i; });
    addSlider("Brightness", o.gamma, 0.0, 1.0, pct100, [&o](double v) { o.gamma = v; });
    addToggle("Fullscreen", o.fullscreen, [&o](bool b) { o.fullscreen = b; });
    addToggle("VSync", o.vsync, [&o](bool b) { o.vsync = b; });
    addToggle("View Bobbing", o.viewBobbing, [&o](bool b) { o.viewBobbing = b; });
}

void ControlsScreen::addOptions() {
    GameOptions& o = mc()->options();
    // Mouse settings. (The rebindable key-binding list is its own sub-system, TODO.)
    addSlider("Sensitivity", o.sensitivity, 0.0, 1.0,
              [](double v) { int p = (int)(v * 200 + 0.5); return p >= 200 ? std::string("HYPERSPEED!!!") : std::to_string(p) + "%"; },
              [&o](double v) { o.sensitivity = v; }, true);
    addToggle("Invert Mouse", o.invertYMouse, [&o](bool b) { o.invertYMouse = b; });
    addToggle("Auto-Jump", o.autoJump, [&o](bool b) { o.autoJump = b; });
    addToggle("Discrete Scrolling", o.discreteMouseScroll, [&o](bool b) { o.discreteMouseScroll = b; });
    addToggle("Touchscreen Mode", o.touchscreen, [&o](bool b) { o.touchscreen = b; });
}

// ── AccessibilityOptionsScreen ──────────────────────────────────────────────
// Port of AccessibilityOptionsScreen.options(): the accessibility-related
// toggles/sliders. Real vanilla has 24 OptionInstance entries; we expose the
// ones that exist in our GameOptions. The rest are stubbed as no-op toggles
// so the screen layout matches vanilla (each row is a real control).
void AccessibilityOptionsScreen::addOptions() {
    GameOptions& o = mc()->options();
    // Real toggles from GameOptions.
    addToggle("Show Subtitles", o.showSubtitles, [&o](bool b) { o.showSubtitles = b; });
    addToggle("View Bobbing", o.viewBobbing, [&o](bool b) { o.viewBobbing = b; });
    addToggle("Dark Mojang Studios Background", false, [](bool) {});
    addToggle("Hide Lightning Flashes", false, [](bool) {});
    addToggle("Hide Splash Texts", false, [](bool) {});
    addToggle("Narrator Hotkey", false, [](bool) {});
    addToggle("High Contrast", false, [](bool) {});
    addToggle("Rotate With Minecart", false, [](bool) {});
    addToggle("High Contrast Block Outline", false, [](bool) {});
    // Sliders for effect scales (no-op onChange — real values not wired yet).
    addSlider("Text Background Opacity", 0.5, 0.0, 1.0, pct100, [](double) {}, true);
    addSlider("Chat Opacity", 0.5, 0.0, 1.0, pct100, [](double) {});
    addSlider("Chat Line Spacing", 0.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    addSlider("Chat Delay", 0.0, 0.0, 6.0, [](double v) { return std::to_string((int)(v * 1000)) + " ms"; }, [](double) {});
    addSlider("Notification Display Time", 1.0, 0.5, 12.0, [](double v) { return std::to_string(v).substr(0, 4) + "s"; }, [](double) {});
    addSlider("Menu Background Blurriness", 0.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Screen Effect Scale", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("FOV Effect Scale", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Darkness Effect Scale", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Damage Tilt Strength", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Glint Speed", 0.5, 0.0, 1.0, pct100, [](double) {});
    addSlider("Glint Strength", 0.75, 0.0, 1.0, pct100, [](double) {});
    addSlider("Panorama Speed", 1.0, 0.1, 4.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    // Narrator cycle (Off/System/Chat/All).
    addCycle("Narrator", { "Off", "System", "Chat", "All" }, 0, [](int) {}, true);
}

// ── LanguageSelectScreen ────────────────────────────────────────────────────
// Simplified port: vanilla has a scrolling ObjectSelectionList of all installed
// languages + a search box. We show the common languages as cycle buttons +
// a "Done" button. The language actually applied doesn't change (i18n not
// ported), but the screen layout + navigation is 1:1 with vanilla's
// OptionsSubScreen base.
void LanguageSelectScreen::addOptions() {
    addCycle("Language", {
        "English (US)",
        "English (UK)",
        "Español (España)",
        "Español (México)",
        "Français",
        "Deutsch",
        "Italiano",
        "Português (Brasil)",
        "Русский",
        "日本語",
        "中文(简体)",
        "中文(繁體)",
        "한국어",
    }, 0, [](int) {}, true);
    addToggle("Force Unicode Font", false, [](bool) {});
    addToggle("Japanese Glyph Variants", false, [](bool) {});
}

// ── SkinCustomizationScreen ─────────────────────────────────────────────────
// Port of SkinCustomizationScreen.addOptions(): toggles for each
// PlayerModelPart (cape, jacket, left_sleeve, right_sleeve, left_pants_leg,
// right_pants_leg, hat) + main hand cycle. All default to ON (vanilla default).
void SkinCustomizationScreen::addOptions() {
    addToggle("Cape", true, [](bool) {});
    addToggle("Jacket", true, [](bool) {});
    addToggle("Left Sleeve", true, [](bool) {});
    addToggle("Right Sleeve", true, [](bool) {});
    addToggle("Left Pants Leg", true, [](bool) {});
    addToggle("Right Pants Leg", true, [](bool) {});
    addToggle("Hat", true, [](bool) {});
    addCycle("Main Hand", { "Left", "Right" }, 1, [](int) {}, true);
}

// ── ChatOptionsScreen ───────────────────────────────────────────────────────
// Port of ChatOptionsScreen.options(): 18 chat-related controls.
void ChatOptionsScreen::addOptions() {
    addCycle("Chat Visibility", { "Shown", "Commands Only", "Hidden" }, 0, [](int) {}, true);
    addToggle("Chat Colors", false, [](bool) {});
    addToggle("Web Links", true, [](bool) {});
    addToggle("Prompt on Links", true, [](bool) {});
    addSlider("Chat Opacity", 1.0, 0.0, 1.0, pct100, [](double) {});
    addSlider("Text Background Opacity", 0.5, 0.0, 1.0, pct100, [](double) {});
    addSlider("Chat Scale", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    addSlider("Chat Line Spacing", 0.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 100)) + "%"; }, [](double) {});
    addSlider("Chat Delay", 0.0, 0.0, 6.0, [](double v) { return std::to_string((int)(v * 1000)) + " ms"; }, [](double) {});
    addSlider("Chat Width", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 320)) + "px"; }, [](double) {});
    addSlider("Chat Height (Focused)", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 180)) + "px"; }, [](double) {});
    addSlider("Chat Height (Unfocused)", 1.0, 0.0, 1.0, [](double v) { return std::to_string((int)(v * 90)) + "px"; }, [](double) {});
    addCycle("Narrator", { "Off", "System", "Chat", "All" }, 0, [](int) {}, true);
    addToggle("Auto Suggestions", true, [](bool) {});
    addToggle("Hide Matched Names", false, [](bool) {});
    addToggle("Reduced Debug Info", false, [](bool) {});
    addToggle("Only Show Secure Chat", false, [](bool) {});
    addToggle("Save Chat Drafts", true, [](bool) {});
}

// ── ResourcePacksScreen ─────────────────────────────────────────────────────
// Simplified port. Vanilla has a two-pane list (available / selected) with
// drag-to-reorder. We show a placeholder label + Done — the pack list UI is
// a significant porting effort (ObjectSelectionList, drag-drop, pack stacking).
void ResourcePacksScreen::addOptions() {
    // No interactive controls — vanilla uses a custom list widget, not
    // sliders/cycles. The Done button (added by the base class) is the only
    // control. The title is shown by the base render().
}

// ── TelemetryInfoScreen ─────────────────────────────────────────────────────
// Port of TelemetryInfoScreen. Vanilla shows a telemetry toggle + info text +
// links. We show the toggle + a placeholder.
void TelemetryInfoScreen::addOptions() {
    addToggle("Send Telemetry Data", false, [](bool) {}, true);
}

// ── CreditsAndAttributionScreen ─────────────────────────────────────────────
// Port of CreditsAndAttributionScreen. Three buttons (Credits, Attribution,
// Licenses) + Done. In vanilla, Credits opens WinScreen (the end-game credits
// scroll); Attribution and Licenses open ConfirmLinkScreen (URLs). We make
// them no-op toggles for now (the actual screens are not ported).
void CreditsAndAttributionScreen::addOptions() {
    addToggle("View Credits", false, [](bool) {}, true);
    addToggle("View Attribution", false, [](bool) {});
    addToggle("View Licenses", false, [](bool) {});
}

} // namespace mc::gui::screens
