#include "Gui.h"
#include "../client/Minecraft.h"
#include "../assets/AssetManager.h"
#include "../render/opengl/CommandListGL.h"
#include <stb_image.h>

namespace mc::gui {

Gui::Gui(Minecraft* mc) : m_minecraft(mc) {
}

Gui::~Gui() {
    auto* dev = m_minecraft->device();
    if (m_hotbarTex) dev->destroyTexture(m_hotbarTex);
    if (m_selTex) dev->destroyTexture(m_selTex);
    if (m_crosshairTex) dev->destroyTexture(m_crosshairTex);
    if (m_heartTex) dev->destroyTexture(m_heartTex);
    if (m_foodTex) dev->destroyTexture(m_foodTex);
}

void Gui::render(render::GuiGraphics& g, float pt) {
    renderHotbar(g, pt);
    renderStats(g);

    // Crosshair
    int sw = m_minecraft->guiScaledWidth();
    int sh = m_minecraft->guiScaledHeight();
    if (m_crosshairTex) {
        g.blit(m_crosshairTex, (sw - 15) / 2, (sh - 15) / 2, 0, 0, 15, 15, 15, 15);
    }
}

void Gui::renderHotbar(render::GuiGraphics& g, float pt) {
    int sw = m_minecraft->guiScaledWidth();
    int sh = m_minecraft->guiScaledHeight();
    int x = sw / 2 - 91;
    int y = sh - 22;

    if (m_hotbarTex) g.blit(m_hotbarTex, x, y, 0, 0, 182, 22, 182, 22);
    if (m_selTex) {
        int slot = 0; // TODO: get from player
        g.blit(m_selTex, x - 1 + slot * 20, y - 1, 0, 0, 24, 24, 24, 24);
    }
}

void Gui::renderStats(render::GuiGraphics& g) {
    int sw = m_minecraft->guiScaledWidth();
    int sh = m_minecraft->guiScaledHeight();
    int x = sw / 2 - 91;
    int y = sh - 39;

    // Hearts (simplified container/full)
    if (m_heartTex) {
        for (int i = 0; i < 10; ++i) {
            g.blit(m_heartTex, x + i * 8, y, 0, 0, 9, 9, 9, 9);
        }
    }

    // Food
    if (m_foodTex) {
        int fx = sw / 2 + 1;
        for (int i = 0; i < 10; ++i) {
            g.blit(m_foodTex, fx + i * 8, y, 0, 0, 9, 9, 9, 9);
        }
    }
}

} // namespace mc::gui
