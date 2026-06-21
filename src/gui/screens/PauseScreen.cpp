#include "PauseScreen.h"
#include "options/OptionsScreen.h"
#include "../../client/Minecraft.h"
#include <windows.h>

namespace mc::gui::screens {

PauseScreen::PauseScreen() : Screen("Game Menu") {
}

components::Button* PauseScreen::addButton(int x, int y, int w, int h,
                                           const std::string& text, std::function<void()> onClick) {
    auto btn = std::make_unique<components::Button>(x, y, w, h, text, std::move(onClick));
    btn->setTextures(m_btnNormal, m_btnHighlight);
    components::Button* ptr = btn.get();
    m_buttons.push_back(std::move(btn));
    return ptr;
}

void PauseScreen::init(Minecraft* mc, int w, int h) {
    Screen::init(mc, w, h);
    m_buttons.clear();

    const int center = w / 2;
    const int top = h / 4 + 8;
    const int spacing = 24;

    addButton(center - 102, top + 0 * spacing, 204, 20, "Back to Game",
              [this]() { m_minecraft->resumeGame(); });
    addButton(center - 102, top + 1 * spacing, 98, 20, "Advancements", []() {});
    addButton(center + 4,   top + 1 * spacing, 98, 20, "Statistics", []() {});
    addButton(center - 102, top + 2 * spacing, 98, 20, "Give Feedback", []() {});
    addButton(center + 4,   top + 2 * spacing, 98, 20, "Report Bugs", []() {});
    addButton(center - 102, top + 3 * spacing, 98, 20, "Options...",
              [this]() {
                  Minecraft* mc = m_minecraft;
                  auto options = std::make_unique<OptionsScreen>();
                  options->setButtonTextures(m_btnNormal, m_btnHighlight);
                  options->setBackAction([mc]() { mc->openPauseScreen(); });
                  mc->setScreen(std::move(options));
              });
    addButton(center + 4,   top + 3 * spacing, 98, 20, "Player Reporting", []() {});
    addButton(center - 102, top + 4 * spacing, 204, 20, "Disconnect",
              [this]() {
                  m_minecraft->disconnect();
                  m_minecraft->openTitleScreen();
              });
}

void PauseScreen::render(render::GuiGraphics& g, int mx, int my, float) {
    g.fill(0, 0, m_width, m_height, { 0.0f, 0.0f, 0.0f, 0.55f });
    render::Font* font = m_minecraft->font();
    if (font) {
        const int tw = font->width(m_title);
        font->drawString(g, m_title, (float)(m_width / 2 - tw / 2), 40.0f, { 1, 1, 1, 1 });
        for (auto& b : m_buttons) b->render(g, *font, mx, my);
    }
}

void PauseScreen::mouseClicked(double x, double y, int button) {
    std::function<void()> action;
    for (auto& b : m_buttons) {
        action = b->clickAction(x, y, button);
        if (action) break;
    }
    if (action) action();
}

void PauseScreen::keyPressed(int key, int, int) {
    if (key == VK_ESCAPE) {
        m_minecraft->resumeGame();
    }
}

} // namespace mc::gui::screens
