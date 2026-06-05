#include "TitleScreen.h"
#include "../../client/Minecraft.h"
#include <chrono>
#include <cmath>

// 1:1 port of net.minecraft.client.gui.screens.TitleScreen (non-demo path).
// Layout, button order, sizes, strings and text positions come straight from the
// decompiled TitleScreen.java + LogoRenderer.java; nothing is invented. Actions
// that target screens not ported yet (Multiplayer/Realms/Options/Language/
// Accessibility) are hard no-ops — NOT fake behaviour. Singleplayer starts the
// local world as a stand-in for the unported SelectWorldScreen. Background: the
// title panorama art is shipped as 1x1 stubs in 26.1.2's jar, so this uses the
// dirt-tile MENU_BACKGROUND fallback (the game's own no-panorama background).

namespace mc::gui::screens {

TitleScreen::TitleScreen() : Screen("Title Screen") {
}

TitleScreen::~TitleScreen() {
    // Textures owned by Minecraft.
}

components::Button* TitleScreen::addButton(int x, int y, int w, int h,
                                           const std::string& text, std::function<void()> onClick) {
    auto btn = std::make_unique<components::Button>(x, y, w, h, text, std::move(onClick));
    btn->setTextures(m_btnNormal, m_btnHighlight);
    components::Button* ptr = btn.get();
    m_buttons.push_back(std::move(btn));
    return ptr;
}

void TitleScreen::init(Minecraft* mc, int w, int h) {
    Screen::init(mc, w, h);
    m_buttons.clear();

    const int spacing = 24;
    int topPos = h / 4 + 48;

    // createNormalMenuOptions(topPos, 24): Singleplayer / Multiplayer / Realms.
    addButton(w / 2 - 100, topPos, 200, 20, "Singleplayer",
              [this]() { m_minecraft->startLocalGame(0); });
    addButton(w / 2 - 100, topPos + spacing, 200, 20, "Multiplayer", []() {});      // JoinMultiplayerScreen — not ported
    addButton(w / 2 - 100, topPos + 2 * spacing, 200, 20, "Minecraft Realms", []() {}); // RealmsMainScreen — not ported
    topPos = topPos + 2 * spacing; // y of the Realms button (createNormalMenuOptions return)

    // createTestWorldButton is IDE-only → skipped.

    // Language icon, then Options / Quit, then Accessibility icon — one row.
    topPos += 36;
    addButton(w / 2 - 124, topPos, 20, 20, "", []() {});                              // CommonButtons.language — not ported
    addButton(w / 2 - 100, topPos, 98, 20, "Options...", []() {});                    // OptionsScreen — not ported yet
    addButton(w / 2 + 2, topPos, 98, 20, "Quit Game", []() { PostQuitMessage(0); });
    addButton(w / 2 + 104, topPos, 20, 20, "", []() {});                              // CommonButtons.accessibility — not ported
}

void TitleScreen::renderDirtBackground(render::GuiGraphics& g) {
    if (!m_dirtTex) { g.fill(0, 0, m_width, m_height, { 0.1f, 0.1f, 0.1f, 1.0f }); return; }
    // Tile the 16x16 dirt at 32px (shown 2x, classic blocky look), darkened to ~25%
    // — the game's own no-panorama MENU_BACKGROUND. texW/texH=32 maps UV [0,1] across
    // each 32px tile, so one dirt cell fills each tile.
    constexpr int TILE = 32;
    const glm::vec4 dark{ 0.25f, 0.25f, 0.25f, 1.0f };
    for (int y = 0; y < m_height; y += TILE)
        for (int x = 0; x < m_width; x += TILE)
            g.blitAlpha(m_dirtTex, x, y, 0.0f, 0.0f, TILE, TILE, dark, TILE, TILE);
}

void TitleScreen::renderSplash(render::GuiGraphics& g) {
    if (m_splash.empty()) return;
    render::Font* font = m_minecraft->font();
    if (!font) return;
    // Port of SplashRenderer.extractRenderState: yellow text, rotated -20deg, pulsing.
    const float PI = 3.14159265358979323846f;
    const int textWidth = font->width(m_splash);
    const long millis = (long)(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now().time_since_epoch()).count() % 1000);
    const float textPhase = 1.8f - std::fabs(std::sin((float)millis / 1000.0f * (PI * 2.0f)) * 0.1f);
    const float textScale = textPhase * 100.0f / (textWidth + 32);
    g.push();
    g.translate(m_width / 2.0f + 123.0f, 69.0f);
    g.rotate(-PI / 9.0f);
    g.scale(textScale, textScale);
    font->drawString(g, m_splash, (float)(-textWidth / 2), -8.0f, { 1.0f, 1.0f, 0.0f, 1.0f });
    g.pop();
}

void TitleScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    renderDirtBackground(g);

    // Logo: 256x44 sampled from the top of the 256x64 texture, at (w/2-128, 30).
    if (m_logoTex) {
        g.blit(m_logoTex, m_width / 2 - 128, 30, 0.0f, 0.0f, 256, 44, 256, 64);
    }
    // Edition subtitle ("Java Edition"): 128x14 from 128x16, overlapping the logo by 7.
    if (m_editionTex) {
        g.blit(m_editionTex, m_width / 2 - 64, 30 + 44 - 7, 0.0f, 0.0f, 128, 14, 128, 16);
    }

    renderSplash(g);

    render::Font* font = m_minecraft->font();
    for (auto& b : m_buttons) {
        b->render(g, *font, mx, my);
    }

    // Language / accessibility icons on their 20x20 buttons (16x16 sprite, centred).
    const int rowY = m_height / 4 + 132;
    if (m_langTex)   g.blit(m_langTex,   m_width / 2 - 124 + 2, rowY + 2, 0.0f, 0.0f, 16, 16, 16, 16);
    if (m_accessTex) g.blit(m_accessTex, m_width / 2 + 104 + 2, rowY + 2, 0.0f, 0.0f, 16, 16, 16, 16);

    // Version (bottom-left) and copyright (bottom-right), like TitleScreen.java.
    font->drawString(g, "Minecraft 26.1.2", 2, m_height - 10, { 1, 1, 1, 1 });
    const std::string copyright = "Copyright Mojang AB. Do not distribute!";
    const int cw = font->width(copyright);
    font->drawString(g, copyright, m_width - cw - 2, m_height - 10, { 1, 1, 1, 1 });
}

void TitleScreen::mouseClicked(double x, double y, int button) {
    for (auto& b : m_buttons) {
        if (b->mouseClicked(x, y, button)) break;
    }
}

} // namespace mc::gui::screens
