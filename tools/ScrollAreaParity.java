// Ground truth for the pure scroll/scrollbar math of net.minecraft.client.gui.components.
// AbstractScrollArea. Drives a REAL concrete AbstractScrollArea subclass (settable contentHeight,
// no-op render/narration) so maxScrollAmount/scrollerHeight/scrollBarX/scrollBarY/setScrollAmount/
// isOverScrollbar/mouseScrolled are the genuine bytecode. GL render methods are not invoked.
//
//   tools/run_groundtruth.ps1 -Tool ScrollAreaParity -Out mcpp/build/scroll_area.tsv
//
// Rows (cfg = x y w h contentH scrollRate):
//   AREA  <cfg>  <maxScroll> <scrollable> <scrollerHeight> <scrollBarX> <scrollbarWidth> <scrollRateBits>
//   SCROLL <cfg> <setValBits>  <clampedAmountBits> <scrollBarY>
//   OVER  <cfg> <pxBits> <pyBits>  <isOverScrollbar>
//   WHEEL <cfg> <baseSetBits> <scrollYBits>  <resultAmountBits>

import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.components.AbstractScrollArea;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.network.chat.Component;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class ScrollAreaParity {
    static final java.io.PrintStream O = System.out;
    static int b(boolean x) { return x ? 1 : 0; }
    static String d(double v) { return Long.toString(Double.doubleToRawLongBits(v)); }

    // Concrete, GL-free AbstractScrollArea: settable contentHeight; protected gates bridged.
    static final class A extends AbstractScrollArea {
        private final int contentH;
        A(int x, int y, int w, int h, int contentH, int scrollRate) {
            super(x, y, w, h, Component.empty(), AbstractScrollArea.defaultSettings(scrollRate));
            this.contentH = contentH;
        }
        @Override protected int contentHeight() { return contentH; }
        @Override protected void extractWidgetRenderState(GuiGraphicsExtractor g, int mx, int my, float a) {}
        @Override protected void updateWidgetNarration(NarrationElementOutput o) {}
        boolean scrollableB() { return scrollable(); }
        int scrollerHeightB() { return scrollerHeight(); }
        int scrollBarXB() { return scrollBarX(); }
        double scrollRateB() { return scrollRate(); }
        boolean isOverScrollbarB(double x, double y) { return isOverScrollbar(x, y); }
    }

    // x, y, w, h, contentHeight, scrollRate
    static final int[][] CFG = {
        {0, 0, 200, 100, 300, 10},   // scrollable, max=200
        {0, 0, 200, 100, 50, 10},    // content < height -> max=0, not scrollable
        {10, 20, 150, 80, 80, 7},    // content == height -> max=0
        {5, 5, 100, 40, 1000, 20},   // tall content, height-8=32
        {0, 0, 200, 36, 400, 10},    // height 36 < 40 -> height-8=28 < 32 (min>max clamp)
        {-50, -30, 220, 120, 117, 14},
        {0, 0, 200, 200, 1, 10}      // tiny content -> scrollerHeight clamps to height-8
    };
    static final double[] SETS = {-50.0, 0.0, 0.4, 37.9, 99.0, 100.5, 150.0, 199.999, 200.0, 250.0};
    static final double[] WHEELS = {1.0, -1.0, 3.0, -2.5, 0.0};

    static A make(int[] c) { return new A(c[0], c[1], c[2], c[3], c[4], c[5]); }

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        for (int[] c : CFG) {
            A a = make(c);
            StringBuilder sb = new StringBuilder("AREA");
            for (int v : c) sb.append('\t').append(v);
            sb.append('\t').append(a.maxScrollAmount()).append('\t').append(b(a.scrollableB()))
              .append('\t').append(a.scrollerHeightB()).append('\t').append(a.scrollBarXB())
              .append('\t').append(a.scrollbarWidth()).append('\t').append(d(a.scrollRateB()));
            O.println(sb);
        }

        for (int[] c : CFG) {
            for (double s : SETS) {
                A a = make(c);
                a.setScrollAmount(s);
                StringBuilder sb = new StringBuilder("SCROLL");
                for (int v : c) sb.append('\t').append(v);
                sb.append('\t').append(d(s)).append('\t').append(d(a.scrollAmount()))
                  .append('\t').append(a.scrollBarY());
                O.println(sb);
            }
        }

        for (int[] c : CFG) {
            A a = make(c);
            int sbx = a.scrollBarXB();
            double[][] pts = {
                {sbx, c[1]}, {sbx + 6, c[1] + c[3] - 1}, {sbx - 1, c[1] + 5},
                {sbx + 7, c[1] + 5}, {sbx + 3, c[1] - 1}, {sbx + 3, c[1] + c[3]},
                {sbx + 3, c[1] + c[3] / 2.0}, {sbx + 3.0, c[1] + 0.0}
            };
            for (double[] p : pts) {
                StringBuilder sb = new StringBuilder("OVER");
                for (int v : c) sb.append('\t').append(v);
                sb.append('\t').append(d(p[0])).append('\t').append(d(p[1]))
                  .append('\t').append(b(a.isOverScrollbarB(p[0], p[1])));
                O.println(sb);
            }
        }

        for (int[] c : CFG) {
            for (double base : new double[]{0.0, 100.0, 200.0}) {
                for (double w : WHEELS) {
                    A a = make(c);
                    a.setScrollAmount(base);
                    a.mouseScrolled(0.0, 0.0, 0.0, w);
                    StringBuilder sb = new StringBuilder("WHEEL");
                    for (int v : c) sb.append('\t').append(v);
                    sb.append('\t').append(d(base)).append('\t').append(d(w))
                      .append('\t').append(d(a.scrollAmount()));
                    O.println(sb);
                }
            }
        }
    }
}
