#pragma once
#include "../../render/gui/GuiGraphics.h"
#include "../../render/gui/Font.h"
#include <string>

namespace mc {
class Minecraft;
}

namespace mc::gui {

// Port of net.minecraft.client.gui.screens.Screen
class Screen {
public:
    explicit Screen(const std::string& title) : m_title(title) {}
    virtual ~Screen() = default;

    virtual void init(Minecraft* mc, int w, int h) {
        m_minecraft = mc;
        m_width = w;
        m_height = h;
    }

    virtual void render(render::GuiGraphics& g, int mouseX, int mouseY, float partialTick) = 0;
    virtual void keyPressed(int key, int scancode, int mods) {}
    virtual void mouseClicked(double x, double y, int button) {}
    virtual void mouseReleased(double x, double y, int button) {}
    virtual void mouseDragged(double x, double y, int button, double dx, double dy) {}
    virtual void mouseScrolled(double x, double y, double dx, double dy) {}

    const std::string& title() const { return m_title; }

protected:
    Minecraft*  m_minecraft = nullptr;
    std::string m_title;
    int         m_width = 0;
    int         m_height = 0;
};

} // namespace mc::gui
