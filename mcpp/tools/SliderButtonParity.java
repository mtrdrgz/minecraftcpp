// Ground truth for the GL-free value/mouse/handle/keyboard math of
// net.minecraft.client.gui.components.AbstractSliderButton. Drives a REAL concrete subclass (no-op
// applyValue/updateMessage). setValue/onClick(setValueFromMouse)/keyPressed are the genuine bytecode;
// the handle X expression is hoisted verbatim from extractWidgetRenderState (line 72) into a bridge so
// the render math is certified without invoking GL. canChangeValue is set directly (its only setter,
// setFocused, derefs Minecraft).
//
//   tools/run_groundtruth.ps1 -Tool SliderButtonParity -Out mcpp/build/slider_button.tsv
//
// Rows:
//   SETVAL <x> <w> <newValBits>             <valueBits> <handleX>
//   MOUSE  <x> <w> <mouseXBits>             <valueBits> <handleX>
//   KEY    <x> <w> <initValBits> <left:0/1> <valueBits> <handleX>

import net.minecraft.client.gui.components.AbstractSliderButton;
import net.minecraft.client.input.KeyEvent;
import net.minecraft.client.input.MouseButtonEvent;
import net.minecraft.client.input.MouseButtonInfo;
import net.minecraft.network.chat.Component;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class SliderButtonParity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return Long.toString(Double.doubleToRawLongBits(v)); }
    static double bd(long b) { return Double.longBitsToDouble(b); }

    static final class Sl extends AbstractSliderButton {
        Sl(int x, int y, int w, int h, double v) { super(x, y, w, h, Component.empty(), v); }
        @Override protected void updateMessage() {}
        @Override protected void applyValue() {}
        void setValueB(double v) { setValue(v); }
        double valueB() { return this.value; }
        int handleX() { return getX() + (int)(this.value * (this.width - 8)); }  // == render line 72
        void setCanChange(boolean b) { this.canChangeValue = b; }
    }

    static final int[][] XW = { {0, 100}, {10, 150}, {-5, 200}, {100, 9}, {0, 50} };
    static final long[] NEWVALS = mapBits(new double[]{-0.5, 0.0, 0.3, 0.5, 0.7, 1.0, 1.5});
    static final long[] MOUSEX  = mapBits(new double[]{-10.0, 0.0, 4.0, 50.0, 53.5, 100.0, 250.0, 8.9});
    static final long[] INITVAL = mapBits(new double[]{0.0, 0.3, 0.5, 1.0});

    static long[] mapBits(double[] a) {
        long[] o = new long[a.length];
        for (int i = 0; i < a.length; i++) o[i] = Double.doubleToRawLongBits(a[i]);
        return o;
    }

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        for (int[] xw : XW) {
            for (long nb : NEWVALS) {
                Sl s = new Sl(xw[0], 0, xw[1], 20, 0.0);
                s.setValueB(bd(nb));
                O.println("SETVAL\t" + xw[0] + "\t" + xw[1] + "\t" + nb + "\t" + d(s.valueB()) + "\t" + s.handleX());
            }
        }

        for (int[] xw : XW) {
            for (long mb : MOUSEX) {
                Sl s = new Sl(xw[0], 0, xw[1], 20, 0.0);
                s.onClick(new MouseButtonEvent(bd(mb), 0.0, new MouseButtonInfo(0, 0)), false);
                O.println("MOUSE\t" + xw[0] + "\t" + xw[1] + "\t" + mb + "\t" + d(s.valueB()) + "\t" + s.handleX());
            }
        }

        for (int[] xw : XW) {
            for (long ib : INITVAL) {
                for (int left = 0; left <= 1; left++) {
                    Sl s = new Sl(xw[0], 0, xw[1], 20, bd(ib));
                    s.setCanChange(true);
                    s.keyPressed(new KeyEvent(left == 1 ? 263 : 262, 0, 0));  // 263=LEFT, 262=RIGHT
                    O.println("KEY\t" + xw[0] + "\t" + xw[1] + "\t" + ib + "\t" + left
                            + "\t" + d(s.valueB()) + "\t" + s.handleX());
                }
            }
        }
    }
}
