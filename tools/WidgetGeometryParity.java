// Ground truth for the pure geometry / hit-testing / state core of
// net.minecraft.client.gui.components.AbstractWidget. Drives a REAL concrete AbstractWidget subclass
// (no-op render/narration) so getRectangle/getRight/getBottom/isActive/isMouseOver/narrationPriority
// are the genuine bytecode. The GL/sound-coupled true-path of mouseClicked is NOT invoked; instead
// its three composable predicates (isActive, isValidClickButton, isMouseOver) are emitted and the
// C++ side recomposes the boolean decision.
//
//   tools/run_groundtruth.ps1 -Tool WidgetGeometryParity -Out mcpp/build/widget_geometry.tsv
//
// Rows:
//   WIDGET <x y w h> <active visible focused hovered>  <right> <bottom> <rx ry rw rh> <isActive> <isHovered> <isFocused> <hoveredOrFocused> <narrationOrdinal>
//   OVER   <x y w h> <active visible> <mx> <my>  <isMouseOver>
//   CLICK  <x y w h> <active visible> <button> <mx> <my>  <isActive> <validButton> <isMouseOver> <accept>

import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.components.AbstractWidget;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.client.gui.navigation.ScreenRectangle;
import net.minecraft.client.input.MouseButtonInfo;
import net.minecraft.network.chat.Component;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class WidgetGeometryParity {
    static final java.io.PrintStream O = System.out;
    static int b(boolean x) { return x ? 1 : 0; }

    // Concrete, GL-free AbstractWidget: abstract hooks are no-ops; bridges expose protected/private gates.
    static final class W extends AbstractWidget {
        W(int x, int y, int w, int h) { super(x, y, w, h, Component.empty()); }
        @Override protected void extractWidgetRenderState(GuiGraphicsExtractor g, int mx, int my, float a) {}
        @Override protected void updateWidgetNarration(NarrationElementOutput o) {}
        void setHoveredForTest(boolean v) { this.isHovered = v; } // isHovered is protected -> inherited
        boolean validBtn(int btn) { return isValidClickButton(new MouseButtonInfo(btn, 0)); }
    }

    static final int[][] RECTS = {
        {0, 0, 20, 20}, {5, 5, 10, 4}, {-3, -3, 6, 6}, {100, 50, 200, 20}, {0, 0, 1, 1}
    };
    // active, visible, focused, hovered
    static final int[][] FLAGS = {
        {1, 1, 0, 0}, {1, 1, 1, 0}, {1, 1, 0, 1}, {1, 1, 1, 1}, {0, 1, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 1}
    };
    static final double[][] PTS = {
        {0.0, 0.0}, {9.9, 9.9}, {10.0, 10.0}, {19.999, 19.999}, {20.0, 20.0},
        {-1.0, -1.0}, {5.5, 5.5}, {4.999, 4.999}, {14.0, 8.0}, {300.0, 70.0}
    };

    static W make(int[] r, int active, int visible, int focused, int hovered) {
        W w = new W(r[0], r[1], r[2], r[3]);
        w.active = active != 0;
        w.visible = visible != 0;
        w.setFocused(focused != 0);
        w.setHoveredForTest(hovered != 0);
        return w;
    }

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        for (int[] r : RECTS) {
            for (int[] f : FLAGS) {
                W w = make(r, f[0], f[1], f[2], f[3]);
                ScreenRectangle rect = w.getRectangle();
                StringBuilder sb = new StringBuilder("WIDGET");
                for (int v : r) sb.append('\t').append(v);
                for (int v : f) sb.append('\t').append(v);
                sb.append('\t').append(w.getRight()).append('\t').append(w.getBottom())
                  .append('\t').append(rect.left()).append('\t').append(rect.top())
                  .append('\t').append(rect.width()).append('\t').append(rect.height())
                  .append('\t').append(b(w.isActive())).append('\t').append(b(w.isHovered()))
                  .append('\t').append(b(w.isFocused())).append('\t').append(b(w.isHoveredOrFocused()))
                  .append('\t').append(w.narrationPriority().ordinal());
                O.println(sb);
            }
        }

        // OVER: isMouseOver over a matrix of (rect x active/visible x point). focused/hovered irrelevant.
        for (int[] r : RECTS) {
            for (int active = 0; active <= 1; active++) {
                for (int visible = 0; visible <= 1; visible++) {
                    W w = make(r, active, visible, 0, 0);
                    for (double[] p : PTS) {
                        O.println("OVER\t" + r[0] + "\t" + r[1] + "\t" + r[2] + "\t" + r[3]
                                + "\t" + active + "\t" + visible
                                + "\t" + Double.doubleToRawLongBits(p[0]) + "\t" + Double.doubleToRawLongBits(p[1])
                                + "\t" + b(w.isMouseOver(p[0], p[1])));
                    }
                }
            }
        }

        // CLICK decision components for buttons 0,1,2.
        for (int[] r : RECTS) {
            for (int active = 0; active <= 1; active++) {
                W w = make(r, active, 1, 0, 0);
                for (int btn = 0; btn <= 2; btn++) {
                    for (double[] p : PTS) {
                        boolean isAct = w.isActive();
                        boolean valid = w.validBtn(btn);
                        boolean over = w.isMouseOver(p[0], p[1]);
                        boolean accept = isAct && valid && over;
                        O.println("CLICK\t" + r[0] + "\t" + r[1] + "\t" + r[2] + "\t" + r[3]
                                + "\t" + active + "\t1\t" + btn
                                + "\t" + Double.doubleToRawLongBits(p[0]) + "\t" + Double.doubleToRawLongBits(p[1])
                                + "\t" + b(isAct) + "\t" + b(valid) + "\t" + b(over) + "\t" + b(accept));
                    }
                }
            }
        }
    }
}
