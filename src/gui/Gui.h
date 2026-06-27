#pragma once
#include "../render/gui/GuiGraphics.h"
#include "../render/gui/Font.h"

namespace mc {
class Minecraft;
}

namespace mc::gui {

// Port of net.minecraft.client.gui.Gui (HUD)
class Gui {
public:
    Gui(Minecraft* minecraft);
    ~Gui();

    void setHotbarTexture(render::ITexture* t) { m_hotbarTex = t; }
    void setSelectionTexture(render::ITexture* t) { m_selTex = t; }
    void setCrosshairTexture(render::ITexture* t) { m_crosshairTex = t; }
    // Heart sprites: container (black outline background) + full/half foreground.
    // 1:1 with Gui.extractHeart: draws container first, then the heart sprite.
    void setHeartContainerTexture(render::ITexture* t) { m_heartContainerTex = t; }
    void setHeartFullTexture(render::ITexture* t) { m_heartFullTex = t; }
    void setHeartHalfTexture(render::ITexture* t) { m_heartHalfTex = t; }
    // Food sprites: empty (background) + full/half foreground.
    // 1:1 with Gui.extractFood: draws empty first, then the food sprite.
    void setFoodEmptyTexture(render::ITexture* t) { m_foodEmptyTex = t; }
    void setFoodFullTexture(render::ITexture* t) { m_foodFullTex = t; }
    void setFoodHalfTexture(render::ITexture* t) { m_foodHalfTex = t; }

    void render(render::GuiGraphics& g, float partialTick);

private:
    void renderHotbar(render::GuiGraphics& g, float partialTick);
    void renderStats(render::GuiGraphics& g);

    Minecraft* m_minecraft;

    // HUD Textures
    render::ITexture* m_hotbarTex = nullptr;
    render::ITexture* m_selTex = nullptr;
    render::ITexture* m_crosshairTex = nullptr;
    // Hearts: container (outline bg) + full + half
    render::ITexture* m_heartContainerTex = nullptr;
    render::ITexture* m_heartFullTex = nullptr;
    render::ITexture* m_heartHalfTex = nullptr;
    // Food: empty (bg) + full + half
    render::ITexture* m_foodEmptyTex = nullptr;
    render::ITexture* m_foodFullTex = nullptr;
    render::ITexture* m_foodHalfTex = nullptr;
};

} // namespace mc::gui
