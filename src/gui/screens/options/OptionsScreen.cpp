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
    for (auto& widget : m_widgets) if (widget->mouseClicked(x, y, button)) break;
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

    auto add = [&](std::unique_ptr<AbstractWidget> wgt) { wgt->setTextures(m_btnN, m_btnH); m_widgets.push_back(std::move(wgt)); };
    Minecraft* mc = m_minecraft;
    auto openSub = [mc, this](std::unique_ptr<gui::Screen> s) {
        if (auto* sub = dynamic_cast<OptionsSubScreen*>(s.get())) sub->setButtonTextures(m_btnN, m_btnH);
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
    struct Cat { const char* label; int kind; }; // kind: 0=generic,1=sound,2=video,3=controls
    static const Cat CATS[] = {
        { "Skin Customization...", 0 }, { "Music & Sounds...", 1 }, { "Video Settings...", 2 },
        { "Controls...", 3 }, { "Language...", 0 }, { "Chat Settings...", 0 },
        { "Resource Packs...", 0 }, { "Accessibility Settings", 0 }, { "Telemetry Data...", 0 },
        { "Credits & Attribution...", 0 },
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
                if (kind == 1)      s = std::make_unique<SoundOptionsScreen>(title, back);
                else if (kind == 2) s = std::make_unique<VideoSettingsScreen>(title, back);
                else if (kind == 3) s = std::make_unique<ControlsScreen>(title, back);
                else                s = std::make_unique<OptionsSubScreen>(title, back);
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
    for (auto& widget : m_widgets) if (widget->mouseClicked(x, y, button)) break;
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

} // namespace mc::gui::screens
