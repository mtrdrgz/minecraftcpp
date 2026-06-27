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
    if (m_heartContainerTex) dev->destroyTexture(m_heartContainerTex);
    if (m_heartFullTex) dev->destroyTexture(m_heartFullTex);
    if (m_heartHalfTex) dev->destroyTexture(m_heartHalfTex);
    if (m_foodEmptyTex) dev->destroyTexture(m_foodEmptyTex);
    if (m_foodFullTex) dev->destroyTexture(m_foodFullTex);
    if (m_foodHalfTex) dev->destroyTexture(m_foodHalfTex);
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

// 1:1 port of Gui.extractHearts + Gui.extractHeart:
//   for each of 10 heart containers:
//     1. draw heart/container.png (the black outline background) at 9x9
//     2. draw heart/full.png or heart/half.png on top (if health > 0)
// The container sprite IS the "black outline" — it's drawn first as the
// background, then the colored heart sprite is drawn on top. Without the
// container, the hearts have no visible outline (the bug the user reported).
//
// HEART_SIZE = 9, HEART_SEPARATION = 8 (Gui.java:124-125).
// Left edge: x = sw/2 - 91 (same as hotbar left edge).
// Y: sh - 39 (one row above the hotbar).
void Gui::renderStats(render::GuiGraphics& g) {
    int sw = m_minecraft->guiScaledWidth();
    int sh = m_minecraft->guiScaledHeight();
    int x = sw / 2 - 91;
    int y = sh - 39;

    // Hearts: 10 containers (always drawn), then full/half on top.
    // TODO: get real health from player. For now, draw all 10 as full.
    const int currentHealth = 20;  // 10 hearts = 20 health points
    for (int i = 0; i < 10; ++i) {
        const int hx = x + i * 8;
        // 1. Container (the black outline background)
        if (m_heartContainerTex) {
            g.blit(m_heartContainerTex, hx, y, 0, 0, 9, 9, 9, 9);
        }
        // 2. Full or half heart on top
        const int halves = i * 2;
        if (halves + 1 < currentHealth) {
            // Full heart
            if (m_heartFullTex) {
                g.blit(m_heartFullTex, hx, y, 0, 0, 9, 9, 9, 9);
            }
        } else if (halves < currentHealth) {
            // Half heart
            if (m_heartHalfTex) {
                g.blit(m_heartHalfTex, hx, y, 0, 0, 9, 9, 9, 9);
            }
        }
    }

    // Food: 10 empty containers (always drawn), then full/half on top.
    // Food is drawn right-to-left in vanilla (xRight = sw/2 + 91 - 9),
    // but the sprite positions are the same left-to-right for the icon row.
    // TODO: get real food level from player. For now, draw all 10 as full.
    const int foodLevel = 20;  // 10 drumsticks = 20 food points
    int fx = sw / 2 + 91 - 80;  // right-aligned, 10 icons * 8px = 80px wide
    for (int i = 0; i < 10; ++i) {
        const int fxi = fx + (9 - i) * 8;  // right-to-left: i=0 is rightmost
        // 1. Empty (the background outline)
        if (m_foodEmptyTex) {
            g.blit(m_foodEmptyTex, fxi, y, 0, 0, 9, 9, 9, 9);
        }
        // 2. Full or half on top
        const int halves = i * 2;
        if (halves + 1 < foodLevel) {
            if (m_foodFullTex) {
                g.blit(m_foodFullTex, fxi, y, 0, 0, 9, 9, 9, 9);
            }
        } else if (halves < foodLevel) {
            if (m_foodHalfTex) {
                g.blit(m_foodHalfTex, fxi, y, 0, 0, 9, 9, 9, 9);
            }
        }
    }
}

} // namespace mc::gui
