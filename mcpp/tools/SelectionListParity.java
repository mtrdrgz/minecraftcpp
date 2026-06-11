// Ground truth for the GL-free row/layout geometry of net.minecraft.client.gui.components.
// AbstractSelectionList (+ inner Entry). The concrete Entry subclass + the protected geometry methods
// are reached from WITHIN the AbstractSelectionList subclass body (JLS 6.6.2: protected members are
// accessible to subclass code). Drives the REAL class with minecraft=null — every method under test
// is verified to NOT dereference minecraft (only setSelected/scrollToEntry do, never invoked). GL
// render methods are not called.
//
//   tools/run_groundtruth.ps1 -Tool SelectionListParity -Out mcpp/build/selection_list.tsv
//
// Construction mirrors usage: new List(null,w,h,y,deh); updateSizeAndPosition(w,h,x,y); addEntry*(scroll 0);
// setScrollAmount(s). Rows (cfg = x y w h deh nEntries scrollBits):
//   LIST  <cfg>  <contentH> <maxScroll> <scrollerHeight> <scrollBarX> <rowLeft> <rowRight> <rowWidth> <nextY> <scrollAmtBits>
//   ROW   <cfg> <i>  <rowTop> <rowBottom> <ex> <ey> <ew> <eh> <cx> <cy> <ch> <cyMid> <cBottom> <cw> <cxMid> <cRight>
//   EAT   <cfg> <pxBits> <pyBits>  <entryIndex|-1>

import net.minecraft.client.gui.GuiGraphicsExtractor;
import net.minecraft.client.gui.components.AbstractSelectionList;
import net.minecraft.client.gui.narration.NarrationElementOutput;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;

public class SelectionListParity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return Long.toString(Double.doubleToRawLongBits(v)); }

    // Subclass of AbstractSelectionList; the Entry subclass is nested here so the protected inner
    // Entry type is accessible (subclass body), as are the protected geometry methods.
    static final class L extends AbstractSelectionList<L.E> {
        static final class E extends AbstractSelectionList.Entry<E> {
            @Override public void extractContent(GuiGraphicsExtractor g, int mx, int my, boolean hov, float a) {}
        }
        L(int w, int h, int y, int deh) { super(null, w, h, y, deh); }
        @Override protected void updateWidgetNarration(NarrationElementOutput o) {}
        int addRow(E e, int h) { return addEntry(e, h); }
        int contentHeightB() { return contentHeight(); }
        int scrollBarXB() { return scrollBarX(); }
        int scrollerHeightB() { return scrollerHeight(); }
        int entryIndexAt(double px, double py) {
            E e = getEntryAtPosition(px, py);
            return e == null ? -1 : children().indexOf(e);
        }
        E entry(int i) { return children().get(i); }
    }

    // x, y, w, h, defaultEntryHeight
    static final int[][] CFG = {
        {0, 0, 300, 200, 20},     // S1: 5x20 -> content 104, max 0
        {0, 0, 300, 100, 25},     // S2: 8x25 -> content 204, max 104
        {20, 10, 400, 120, 18},   // S3: 10x18 -> content 184, max 64
        {-30, -20, 250, 80, 22},  // S4: varying heights, content 133, max 53
        {5, 5, 320, 150, 20}      // S5: empty
    };
    static final int[][] HEIGHTS = {
        {20, 20, 20, 20, 20},
        {25, 25, 25, 25, 25, 25, 25, 25},
        {18, 18, 18, 18, 18, 18, 18, 18, 18, 18},
        {22, 30, 15, 40, 22},
        {}
    };
    static final double[] SCROLLS = {0.0, 50.0, 30.0, 53.0, 0.0};

    static L build(int s) {
        int[] c = CFG[s];
        L l = new L(c[2], c[3], c[1], c[4]);
        l.updateSizeAndPosition(c[2], c[3], c[0], c[1]);
        for (int h : HEIGHTS[s]) l.addRow(new L.E(), h);
        l.setScrollAmount(SCROLLS[s]);
        return l;
    }

    static String cfgCols(int s) {
        int[] c = CFG[s];
        return c[0] + "\t" + c[1] + "\t" + c[2] + "\t" + c[3] + "\t" + c[4] + "\t" + HEIGHTS[s].length
             + "\t" + d(SCROLLS[s]);
    }

    public static void main(String[] args) {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        for (int s = 0; s < CFG.length; s++) {
            L l = build(s);
            O.println("LIST\t" + cfgCols(s)
                    + "\t" + l.contentHeightB() + "\t" + l.maxScrollAmount() + "\t" + l.scrollerHeightB()
                    + "\t" + l.scrollBarXB() + "\t" + l.getRowLeft() + "\t" + l.getRowRight()
                    + "\t" + l.getRowWidth() + "\t" + l.getNextY() + "\t" + d(l.scrollAmount()));
        }

        for (int s = 0; s < CFG.length; s++) {
            L l = build(s);
            for (int i = 0; i < HEIGHTS[s].length; i++) {
                L.E e = l.entry(i);
                O.println("ROW\t" + cfgCols(s) + "\t" + i
                        + "\t" + l.getRowTop(i) + "\t" + l.getRowBottom(i)
                        + "\t" + e.getX() + "\t" + e.getY() + "\t" + e.getWidth() + "\t" + e.getHeight()
                        + "\t" + e.getContentX() + "\t" + e.getContentY() + "\t" + e.getContentHeight()
                        + "\t" + e.getContentYMiddle() + "\t" + e.getContentBottom() + "\t" + e.getContentWidth()
                        + "\t" + e.getContentXMiddle() + "\t" + e.getContentRight());
            }
        }

        for (int s = 0; s < CFG.length; s++) {
            L l = build(s);
            int rl = l.getRowLeft(), rr = l.getRowRight();
            int cy = CFG[s][1];
            double[][] pts = {
                {rl, cy + 3}, {rr - 1, cy + 3}, {rl - 1, cy + 3}, {rr, cy + 3},
                {(rl + rr) / 2.0, cy + 3}, {(rl + rr) / 2.0, cy + 2}, {(rl + rr) / 2.0, cy - 100},
                {(rl + rr) / 2.0, cy + 40}, {(rl + rr) / 2.0, cy + 90}, {(rl + rr) / 2.0 + 0.9, cy + 3.9}
            };
            for (double[] p : pts) {
                O.println("EAT\t" + cfgCols(s) + "\t" + d(p[0]) + "\t" + d(p[1])
                        + "\t" + l.entryIndexAt(p[0], p[1]));
            }
        }
    }
}
