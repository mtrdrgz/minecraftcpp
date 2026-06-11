#pragma once

// 1:1 port of the pure geometry / hit-testing / activation-state core of
// net.minecraft.client.gui.components.AbstractWidget (AbstractWidget.java). This is the GL-free,
// deterministic part of every GUI widget: position/size, getRectangle/getRight/getBottom,
// active/visible/focused/hovered state, isMouseOver hit-testing, and the click-acceptance decision.
//
// This is the CANONICAL widget-geometry port (sits in mc::gui next to the certified ScreenRectangle).
// The bespoke mc::gui::components::AbstractWidget used by the options screens predates this and should
// converge onto it. Certified by widget_geometry_parity.
//
// 1:1 details captured here:
//   * areCoordinatesInRectangle takes DOUBLE mouse coords compared against int bounds with
//     `x < getRight()` (half-open high edge): a mouse at 9.9 hits a [0,10) rect, 10.0 does not.
//   * isMouseOver folds in isActive() (= visible && active): an inactive/hidden widget is never moused-over.
//   * isValidClickButton accepts ONLY button 0 (left mouse).
//   * mouseClicked's return value is isActive() && isValidClickButton(button) && isMouseOver(x,y)
//     (the true path additionally plays a UI sound + onClick — those are GL/side-effect-coupled and
//     are NOT modelled here; wouldAcceptClick reproduces only the boolean decision).
//   * narrationPriority: focused -> FOCUSED, else hovered -> HOVERED, else NONE.

namespace mc::gui {

// Ordinals match net.minecraft.client.gui.narration.NarratableEntry.NarrationPriority (NONE=0,
// HOVERED=1, FOCUSED=2).
enum class NarrationPriority { NONE = 0, HOVERED = 1, FOCUSED = 2 };

struct WidgetGeometry {
    int x = 0, y = 0, width = 0, height = 0;
    bool active = true;     // AbstractWidget.active (default true)
    bool visible = true;    // AbstractWidget.visible (default true)
    bool focused = false;   // AbstractWidget.focused (default false)
    bool isHovered = false; // AbstractWidget.isHovered (set by extractRenderState; here a plain field)

    int getX() const { return x; }
    int getY() const { return y; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getRight() const { return getX() + getWidth(); }
    int getBottom() const { return getY() + getHeight(); }

    void setX(int v) { x = v; }
    void setY(int v) { y = v; }
    void setPosition(int nx, int ny) { setX(nx); setY(ny); }
    void setSize(int w, int h) { width = w; height = h; }
    void setRectangle(int w, int h, int nx, int ny) { setSize(w, h); setPosition(nx, ny); }
    void setFocused(bool f) { focused = f; }

    // LayoutElement.getRectangle default: new ScreenRectangle(getX(), getY(), getWidth(), getHeight()).
    // Returned as the four ints to avoid a hard dependency on ScreenRectangle.h here; identical values.
    int rectX() const { return getX(); }
    int rectY() const { return getY(); }
    int rectW() const { return getWidth(); }
    int rectH() const { return getHeight(); }

    bool isActive() const { return visible && active; }
    bool isFocused() const { return focused; }
    bool isHoveredFn() const { return isHovered; }
    bool isHoveredOrFocused() const { return isHoveredFn() || isFocused(); }

    // private boolean areCoordinatesInRectangle(double x, double y) — double coords vs int bounds.
    bool areCoordinatesInRectangle(double px, double py) const {
        return px >= getX() && py >= getY() && px < getRight() && py < getBottom();
    }
    // public boolean isMouseOver(double, double)
    bool isMouseOver(double px, double py) const {
        return isActive() && areCoordinatesInRectangle(px, py);
    }
    // protected boolean isValidClickButton(MouseButtonInfo) { return buttonInfo.button() == 0; }
    bool isValidClickButton(int button) const { return button == 0; }

    // Boolean decision of mouseClicked(event, doubleClick) — true iff it would consume the click.
    bool wouldAcceptClick(int button, double px, double py) const {
        return isActive() && isValidClickButton(button) && isMouseOver(px, py);
    }

    NarrationPriority narrationPriority() const {
        if (isFocused()) return NarrationPriority::FOCUSED;
        return isHovered ? NarrationPriority::HOVERED : NarrationPriority::NONE;
    }
};

}  // namespace mc::gui
