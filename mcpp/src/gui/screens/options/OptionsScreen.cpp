#include "OptionsScreen.h"
#include "../../../client/Minecraft.h"

namespace mc::gui::screens {

namespace {
    // The category buttons, in OptionsScreen.java order. {button label, sub-screen title}.
    struct Cat { const char* label; const char* title; };
    const Cat CATEGORIES[] = {
        { "Skin Customization...",   "Skin Customization" },
        { "Music & Sounds...",       "Music & Sounds" },
        { "Video Settings...",       "Video Settings" },
        { "Controls...",             "Controls" },
        { "Language...",             "Language" },
        { "Chat Settings...",        "Chat Settings" },
        { "Resource Packs...",       "Resource Packs" },
        { "Accessibility Settings",  "Accessibility Settings" },
        { "Telemetry Data...",       "Telemetry Data" },
        { "Credits & Attribution...","Credits & Attribution" },
    };

    void drawCentered(render::GuiGraphics& g, render::Font* font, const std::string& s, int cx, int y) {
        if (font) font->drawString(g, s, (float)(cx - font->width(s) / 2), (float)y, { 1, 1, 1, 1 });
    }
}

// ── OptionsScreen ────────────────────────────────────────────────────────────
OptionsScreen::OptionsScreen() : Screen("Options") {}

void OptionsScreen::init(Minecraft* mc, int w, int h) {
    Screen::init(mc, w, h);
    m_buttons.clear();

    const int x0 = w / 2 - 155, x1 = w / 2 + 5; // two 150-wide columns
    const int top = h / 6 + 24;
    auto add = [&](int x, int y, int bw, int bh, const std::string& text, std::function<void()> cb) {
        auto b = std::make_unique<components::Button>(x, y, bw, bh, text, std::move(cb));
        b->setTextures(m_btnN, m_btnH);
        m_buttons.push_back(std::move(b));
    };

    for (int i = 0; i < (int)(sizeof(CATEGORIES) / sizeof(CATEGORIES[0])); ++i) {
        const int col = i % 2, row = i / 2;
        const std::string title = CATEGORIES[i].title;
        add(col == 0 ? x0 : x1, top + row * 24, 150, 20, CATEGORIES[i].label,
            [this, title]() {
                m_minecraft->setScreen(std::make_unique<OptionsSubScreen>(
                    title, m_btnN, m_btnH, [mc = m_minecraft]() { mc->openOptionsScreen(); }));
            });
    }

    // Footer Done (200 wide), returns to the previous screen.
    add(w / 2 - 100, h - 27, 200, 20, "Done", [this]() { if (m_back) m_back(); });
}

void OptionsScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    // Darken the panorama behind for readability (standard menu background dim).
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.6f });
    drawCentered(g, m_minecraft->font(), "Options", m_width / 2, 15);
    render::Font* font = m_minecraft->font();
    for (auto& b : m_buttons) b->render(g, *font, mx, my);
}

void OptionsScreen::mouseClicked(double x, double y, int button) {
    for (auto& b : m_buttons) if (b->mouseClicked(x, y, button)) break;
}

// ── OptionsSubScreen (title + Done; widgets are the next increment) ───────────
OptionsSubScreen::OptionsSubScreen(const std::string& title, render::ITexture* n, render::ITexture* h, std::function<void()> back)
    : Screen(title), m_btnN(n), m_btnH(h), m_back(std::move(back)) {}

void OptionsSubScreen::init(Minecraft* mc, int w, int h) {
    Screen::init(mc, w, h);
    m_buttons.clear();
    auto done = std::make_unique<components::Button>(w / 2 - 100, h - 27, 200, 20, "Done",
                                                     [this]() { if (m_back) m_back(); });
    done->setTextures(m_btnN, m_btnH);
    m_buttons.push_back(std::move(done));
}

void OptionsSubScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.6f });
    drawCentered(g, m_minecraft->font(), m_title, m_width / 2, 15);
    render::Font* font = m_minecraft->font();
    for (auto& b : m_buttons) b->render(g, *font, mx, my);
}

void OptionsSubScreen::mouseClicked(double x, double y, int button) {
    for (auto& b : m_buttons) if (b->mouseClicked(x, y, button)) break;
}

} // namespace mc::gui::screens
