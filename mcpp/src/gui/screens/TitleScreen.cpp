#include "TitleScreen.h"
#include "../../client/Minecraft.h"

namespace mc::gui::screens {

TitleScreen::TitleScreen() : Screen("Title Screen") {
}

TitleScreen::~TitleScreen() {
    // Textures owned by Minecraft or Cache
}

void TitleScreen::init(Minecraft* mc, int w, int h) {
    Screen::init(mc, w, h);
    m_buttons.clear();

    int centerX = w / 2;
    int centerY = h / 2;

    auto btn = std::make_unique<components::Button>(centerX - 100, centerY, 200, 20, "Singleplayer", [this]() {
        m_minecraft->startLocalGame(0);
    });
    btn->setTextures(m_btnNormal, m_btnHighlight);
    m_buttons.push_back(std::move(btn));

    auto local = std::make_unique<components::Button>(centerX - 100, centerY + 24, 200, 20, "Connect to Localhost", [this]() {
        m_minecraft->connectToServer("localhost", 25565, "mcpp_player");
    });
    local->setTextures(m_btnNormal, m_btnHighlight);
    m_buttons.push_back(std::move(local));

    auto quit = std::make_unique<components::Button>(centerX - 100, centerY + 48, 200, 20, "Quit Game", []() {
        PostQuitMessage(0);
    });
    quit->setTextures(m_btnNormal, m_btnHighlight);
    m_buttons.push_back(std::move(quit));
}

void TitleScreen::render(render::GuiGraphics& g, int mx, int my, float pt) {
    // Background (Panorama fallback: solid color or dirt)
    g.fill(0, 0, m_width, m_height, {0.1f, 0.1f, 0.1f, 1.0f});

    // Logo
    if (m_logoTex) {
        g.blit(m_logoTex, m_width / 2 - 128, 30, 0, 0, 256, 64, 256, 64);
    }

    // Buttons
    for (auto& b : m_buttons) {
        b->render(g, *m_minecraft->font(), mx, my);
    }

    // Version
    m_minecraft->font()->drawString(g, "Minecraft 26.1.2 Port (C++)", 2, m_height - 10, {1, 1, 1, 1});
}

void TitleScreen::mouseClicked(double x, double y, int button) {
    for (auto& b : m_buttons) {
        if (b->mouseClicked(x, y, button)) break;
    }
}

} // namespace mc::gui::screens
