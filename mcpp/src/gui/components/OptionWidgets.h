#pragma once
#include "../../render/gui/GuiGraphics.h"
#include "../../render/gui/Font.h"
#include <string>
#include <vector>
#include <functional>

namespace mc::gui::components {

// Common base for option-screen controls (sliders, cycle buttons, plain buttons).
class AbstractWidget {
public:
    AbstractWidget(int x, int y, int w, int h) : m_x(x), m_y(y), m_w(w), m_h(h) {}
    virtual ~AbstractWidget() = default;

    virtual void render(render::GuiGraphics& g, render::Font& font, int mouseX, int mouseY) = 0;
    virtual bool mouseClicked(double x, double y, int button) = 0;

    void setPos(int x, int y) { m_x = x; m_y = y; }
    void setSize(int w, int h) { m_w = w; m_h = h; }
    void setTextures(render::ITexture* n, render::ITexture* hl) { m_texN = n; m_texH = hl; }
    int w() const { return m_w; }
    int h() const { return m_h; }

protected:
    bool hovered(int mx, int my) const { return mx >= m_x && mx < m_x + m_w && my >= m_y && my < m_y + m_h; }
    void drawBg(render::GuiGraphics& g, bool hover);
    void drawLabel(render::GuiGraphics& g, render::Font& font, const std::string& s);

    int m_x, m_y, m_w, m_h;
    render::ITexture* m_texN = nullptr;
    render::ITexture* m_texH = nullptr;
};

// A plain button (Done, category links). Port of Button used on option screens.
class WidgetButton final : public AbstractWidget {
public:
    WidgetButton(int x, int y, int w, int h, std::string label, std::function<void()> onClick)
        : AbstractWidget(x, y, w, h), m_label(std::move(label)), m_onClick(std::move(onClick)) {}
    void render(render::GuiGraphics& g, render::Font& font, int mx, int my) override;
    bool mouseClicked(double x, double y, int button) override;
    void setActive(bool a) { m_active = a; }
private:
    std::string m_label;
    std::function<void()> m_onClick;
    bool m_active = true;
};

// Slider over [minV, maxV]. Click sets the value to the click fraction (and drags
// while held is a future refinement). onChange(value) writes back to GameOptions.
class Slider final : public AbstractWidget {
public:
    Slider(int x, int y, int w, int h, std::string label, double value, double minV, double maxV,
           std::function<std::string(double)> fmt, std::function<void(double)> onChange)
        : AbstractWidget(x, y, w, h), m_label(std::move(label)), m_value(value), m_min(minV), m_max(maxV),
          m_fmt(std::move(fmt)), m_onChange(std::move(onChange)) {}
    void render(render::GuiGraphics& g, render::Font& font, int mx, int my) override;
    bool mouseClicked(double x, double y, int button) override;
private:
    std::string m_label;
    double m_value, m_min, m_max;
    std::function<std::string(double)> m_fmt;
    std::function<void(double)> m_onChange;
};

// Cycle button: clicking advances through `choices`. onChange(index) writes back.
class CycleButton final : public AbstractWidget {
public:
    CycleButton(int x, int y, int w, int h, std::string label, std::vector<std::string> choices,
                int index, std::function<void(int)> onChange)
        : AbstractWidget(x, y, w, h), m_label(std::move(label)), m_choices(std::move(choices)),
          m_index(index), m_onChange(std::move(onChange)) {}
    void render(render::GuiGraphics& g, render::Font& font, int mx, int my) override;
    bool mouseClicked(double x, double y, int button) override;
private:
    std::string m_label;
    std::vector<std::string> m_choices;
    int m_index;
    std::function<void(int)> m_onChange;
};

} // namespace mc::gui::components
