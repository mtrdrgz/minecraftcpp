#include "TitleScreen.h"
#include "options/OptionsScreen.h"
#include "../../client/Minecraft.h"
#include <chrono>
#include <cmath>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

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
              [this]() { m_minecraft->startLocalGameFast(0); });
    addButton(w / 2 - 100, topPos + spacing, 200, 20, "Multiplayer", []() {});      // JoinMultiplayerScreen — not ported
    addButton(w / 2 - 100, topPos + 2 * spacing, 200, 20, "Minecraft Realms", []() {}); // RealmsMainScreen — not ported
    topPos = topPos + 2 * spacing; // y of the Realms button (createNormalMenuOptions return)

    // createTestWorldButton is IDE-only → skipped.

    // Language icon, then Options / Quit, then Accessibility icon — one row.
    topPos += 36;
    // Language button → opens LanguageSelectScreen (1:1 with vanilla
    // TitleScreen: CommonButtons.language → setScreen(new LanguageSelectScreen(...))).
    addButton(w / 2 - 124, topPos, 20, 20, "", [this]() {
        auto s = std::make_unique<gui::screens::LanguageSelectScreen>("Language", [this]() { m_minecraft->openTitleScreen(); });
        s->setButtonTextures(m_btnNormal, m_btnHighlight);
        s->setSliderTextures(m_minecraft->sliderTrackTex(), m_minecraft->sliderHandleTex(), m_minecraft->sliderHandleHlTex());
        s->setListTextures(m_minecraft->headerSepTex(), m_minecraft->footerSepTex(),
                           m_minecraft->scrollerTex(), m_minecraft->scrollerBgTex(), m_minecraft->listBgTex());
        m_minecraft->setScreen(std::move(s));
    });
    addButton(w / 2 - 100, topPos, 98, 20, "Options...", [this]() { m_minecraft->openOptionsScreen(); });
    addButton(w / 2 + 2, topPos, 98, 20, "Quit Game", []() {
#ifdef _WIN32
        PostQuitMessage(0);
#else
        std::exit(0);
#endif
    });
    // Accessibility button → opens AccessibilityOptionsScreen (1:1 with vanilla
    // TitleScreen: CommonButtons.accessibility → setScreen(new AccessibilityOptionsScreen(...))).
    addButton(w / 2 + 104, topPos, 20, 20, "", [this]() {
        auto s = std::make_unique<gui::screens::AccessibilityOptionsScreen>("Accessibility", [this]() { m_minecraft->openTitleScreen(); });
        s->setButtonTextures(m_btnNormal, m_btnHighlight);
        s->setSliderTextures(m_minecraft->sliderTrackTex(), m_minecraft->sliderHandleTex(), m_minecraft->sliderHandleHlTex());
        s->setListTextures(m_minecraft->headerSepTex(), m_minecraft->footerSepTex(),
                           m_minecraft->scrollerTex(), m_minecraft->scrollerBgTex(), m_minecraft->listBgTex());
        m_minecraft->setScreen(std::move(s));
    });
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
    // The rotating panorama (3D cube) is drawn to the command list before this by the
    // main loop; here we just overlay the vignette. If the panorama art isn't loaded,
    // fall back to the dirt MENU_BACKGROUND.
    if (m_minecraft->panoramaLoaded()) {
        if (render::ITexture* ov = m_minecraft->panoramaOverlay())
            g.blit(ov, 0, 0, 0.0f, 0.0f, m_width, m_height, m_width, m_height);
    } else {
        renderDirtBackground(g);
    }

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

    // Language / accessibility icons on their 20x20 buttons.
    // CommonButtons.java: both sprites are 15x15 (icon/language.png,
    // icon/accessibility.png). Vanilla centers them in the 20x20 button with
    // (20-15)/2 = 2.5 → 2px inset. The previous code blitted 16x16 from a 15x15
    // texture, which sampled 1px past the edge and cut off the icon's right/
    // bottom side. Using the real 15x15 source size + 9-slice-style centering
    // fixes the "cut off" look.
    const int rowY = m_height / 4 + 132;
    if (m_langTex)   g.blitSized(m_langTex,   m_width / 2 - 124 + 2, rowY + 2, 16, 16, 0, 0, 15, 15, 15, 15);
    if (m_accessTex) g.blitSized(m_accessTex, m_width / 2 + 104 + 2, rowY + 2, 16, 16, 0, 0, 15, 15, 15, 15);

    // Version (bottom-left) and copyright (bottom-right), like TitleScreen.java.
    font->drawString(g, "Minecraft 26.1.2", 2, m_height - 10, { 1, 1, 1, 1 });
    const std::string copyright = "Copyright Mojang AB. Do not distribute!";
    const int cw = font->width(copyright);
    font->drawString(g, copyright, m_width - cw - 2, m_height - 10, { 1, 1, 1, 1 });
}

void TitleScreen::mouseClicked(double x, double y, int button) {
    std::function<void()> action;
    for (auto& b : m_buttons) {
        action = b->clickAction(x, y, button);
        if (action) break;
    }
    if (action) action();
}

} // namespace mc::gui::screens
