#pragma once

// 1:1 port of the GL-free value / mouse / handle / keyboard math of
// net.minecraft.client.gui.components.AbstractSliderButton (AbstractSliderButton.java) — behind every
// slider (volume, render distance, FOV, ...). Pure arithmetic; the sprite/text render is GL-coupled
// and omitted. Certified by slider_button_parity.
//
// 1:1 details captured here:
//   * value is clamped to [0,1] as DOUBLES (Mth.clamp(double)).
//   * setValueFromMouse: value = (mouseX - (getX()+4)) / (width-8) — mouseX double, (getX()+4) and
//     (width-8) ints promoted to double, then clamp. (onClick does this with no GL.)
//   * handle X (render): getX() + (int)(value * (width-8)) — value*(width-8) is DOUBLE, then (int)
//     TRUNCATE toward zero. HANDLE_WIDTH=8, HANDLE_HALF_WIDTH=4.
//   * keyboard arrow step: value + direction/(width-8) where direction is a FLOAT (+/-1.0F) and
//     (width-8) is int -> the division is computed in SINGLE precision, then widened to double and
//     added to the double value, then clamp. (NOT a double 1.0/(width-8) step.)
//   * width must be > 8 in practice (width==8 divides by zero); not modelled.

#include "../../world/level/levelgen/Mth.h"

namespace mc::gui {

namespace slidermth = mc::levelgen::mth;

struct SliderButton {
    int x = 0, y = 0, width = 0, height = 0;
    bool active = true;
    double value = 0.0;

    int getX() const { return x; }
    int getY() const { return y; }

    void setValue(double newValue) { value = slidermth::clamp(newValue, 0.0, 1.0); }

    // setValueFromMouse(event) == onClick's effect (onClick also sets dragging=active, no value impact).
    void setValueFromMouse(double mouseX) {
        setValue((mouseX - static_cast<double>(getX() + 4)) / static_cast<double>(width - 8));
    }

    // extractWidgetRenderState handle X position.
    int handleX() const { return getX() + static_cast<int>(value * static_cast<double>(width - 8)); }

    // keyPressed arrow step (canChangeValue must be true; here unconditional). left -> -, right -> +.
    void keyStep(bool left) {
        float direction = left ? -1.0f : 1.0f;
        setValue(value + static_cast<double>(direction / static_cast<float>(width - 8)));
    }
};

}  // namespace mc::gui
