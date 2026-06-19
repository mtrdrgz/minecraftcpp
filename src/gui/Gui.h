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
    void setHeartTexture(render::ITexture* t) { m_heartTex = t; }
    void setFoodTexture(render::ITexture* t) { m_foodTex = t; }

    void render(render::GuiGraphics& g, float partialTick);

private:
    void renderHotbar(render::GuiGraphics& g, float partialTick);
    void renderStats(render::GuiGraphics& g);

    Minecraft* m_minecraft;
    
    // HUD Textures
    render::ITexture* m_hotbarTex = nullptr;
    render::ITexture* m_selTex = nullptr;
    render::ITexture* m_crosshairTex = nullptr;
    render::ITexture* m_heartTex = nullptr;
    render::ITexture* m_foodTex = nullptr;
};

} // namespace mc::gui
