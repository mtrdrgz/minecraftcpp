// Ground truth for net.minecraft.client.gui.layouts.GridLayout.arrangeElements (the GUI layout
// foundation). Drives the REAL GridLayout with SpacerElement children (pure geometry, no GL), reads
// each child's computed (x,y). Deterministic int math.
//
//   tools/run_groundtruth.ps1 -Tool GridLayoutParity -Out mcpp/build/grid_layout.tsv
//
// Per case:
//   CASE  gridX gridY rowSpacing colSpacing  childCount
//   CH    row col occRows occCols childW childH padL padT padR padB alignXBits alignYBits   (input)
//   ... childCount CH lines ...
//   POS   <x> <y>   (one per child, in add order)
//   END

import net.minecraft.client.gui.layouts.FrameLayout;
import net.minecraft.client.gui.layouts.GridLayout;
import net.minecraft.client.gui.layouts.LayoutSettings;
import net.minecraft.client.gui.layouts.LinearLayout;
import net.minecraft.client.gui.layouts.SpacerElement;

public class GridLayoutParity {
    static final java.io.PrintStream O = System.out;

    record Ch(int row, int col, int occRows, int occCols, int w, int h,
              int padL, int padT, int padR, int padB, float ax, float ay) {}

    static void emit(int gx, int gy, int rs, int cs, Ch[] specs) {
        O.println("CASE\t" + gx + "\t" + gy + "\t" + rs + "\t" + cs + "\t" + specs.length);
        GridLayout grid = new GridLayout(gx, gy);
        grid.rowSpacing(rs).columnSpacing(cs);
        SpacerElement[] spacers = new SpacerElement[specs.length];
        for (int i = 0; i < specs.length; i++) {
            Ch c = specs[i];
            SpacerElement sp = new SpacerElement(c.w(), c.h());
            spacers[i] = sp;
            LayoutSettings ls = LayoutSettings.defaults()
                    .padding(c.padL(), c.padT(), c.padR(), c.padB())
                    .align(c.ax(), c.ay());
            grid.addChild(sp, c.row(), c.col(), c.occRows(), c.occCols(), ls);
            O.println("CH\t" + c.row() + "\t" + c.col() + "\t" + c.occRows() + "\t" + c.occCols()
                    + "\t" + c.w() + "\t" + c.h() + "\t" + c.padL() + "\t" + c.padT() + "\t" + c.padR() + "\t" + c.padB()
                    + "\t" + String.format("%08x", Float.floatToRawIntBits(c.ax()))
                    + "\t" + String.format("%08x", Float.floatToRawIntBits(c.ay())));
        }
        grid.arrangeElements();
        for (SpacerElement sp : spacers) O.println("POS\t" + sp.getX() + "\t" + sp.getY());
        O.println("END");
    }

    // FrameLayout: overlapping children aligned within the resolved frame size.
    static void emitFrame(int fx, int fy, int minW, int minH, Ch[] specs) {
        O.println("FRAME\t" + fx + "\t" + fy + "\t" + minW + "\t" + minH + "\t" + specs.length);
        FrameLayout frame = new FrameLayout(fx, fy, minW, minH);
        SpacerElement[] spacers = new SpacerElement[specs.length];
        for (int i = 0; i < specs.length; i++) {
            Ch c = specs[i];
            SpacerElement sp = new SpacerElement(c.w(), c.h());
            spacers[i] = sp;
            LayoutSettings ls = LayoutSettings.defaults()
                    .padding(c.padL(), c.padT(), c.padR(), c.padB())
                    .align(c.ax(), c.ay());
            frame.addChild(sp, ls);
            O.println("FCH\t" + c.w() + "\t" + c.h() + "\t" + c.padL() + "\t" + c.padT() + "\t" + c.padR() + "\t" + c.padB()
                    + "\t" + String.format("%08x", Float.floatToRawIntBits(c.ax()))
                    + "\t" + String.format("%08x", Float.floatToRawIntBits(c.ay())));
        }
        frame.arrangeElements();
        for (SpacerElement sp : spacers) O.println("FPOS\t" + sp.getX() + "\t" + sp.getY());
        O.println("FEND");
    }

    // LinearLayout: a 1xN (horizontal) or Nx1 (vertical) grid wrapper.
    static void emitLinear(boolean horizontal, int x, int y, int spacing, Ch[] specs) {
        O.println("LIN\t" + (horizontal ? 1 : 0) + "\t" + x + "\t" + y + "\t" + spacing + "\t" + specs.length);
        LinearLayout lin = horizontal ? LinearLayout.horizontal() : LinearLayout.vertical();
        lin.spacing(spacing);
        lin.setX(x);
        lin.setY(y);
        SpacerElement[] spacers = new SpacerElement[specs.length];
        for (int i = 0; i < specs.length; i++) {
            Ch c = specs[i];
            SpacerElement sp = new SpacerElement(c.w(), c.h());
            spacers[i] = sp;
            LayoutSettings ls = LayoutSettings.defaults()
                    .padding(c.padL(), c.padT(), c.padR(), c.padB())
                    .align(c.ax(), c.ay());
            lin.addChild(sp, ls);
            O.println("LCH\t" + c.w() + "\t" + c.h() + "\t" + c.padL() + "\t" + c.padT() + "\t" + c.padR() + "\t" + c.padB()
                    + "\t" + String.format("%08x", Float.floatToRawIntBits(c.ax()))
                    + "\t" + String.format("%08x", Float.floatToRawIntBits(c.ay())));
        }
        lin.arrangeElements();
        for (SpacerElement sp : spacers) O.println("LPOS\t" + sp.getX() + "\t" + sp.getY());
        O.println("LEND");
    }

    public static void main(String[] args) {
        // 1: simple 1x3 row, uniform spacers, spacing 4.
        emit(10, 20, 0, 4, new Ch[]{
            new Ch(0, 0, 1, 1, 16, 12, 0, 0, 0, 0, 0f, 0f),
            new Ch(0, 1, 1, 1, 16, 12, 0, 0, 0, 0, 0f, 0f),
            new Ch(0, 2, 1, 1, 16, 12, 0, 0, 0, 0, 0f, 0f)});
        // 2: 2x2 grid, varied sizes (column widths = max), spacing 2x3.
        emit(0, 0, 2, 3, new Ch[]{
            new Ch(0, 0, 1, 1, 20, 10, 0, 0, 0, 0, 0f, 0f),
            new Ch(0, 1, 1, 1, 8, 30, 0, 0, 0, 0, 0f, 0f),
            new Ch(1, 0, 1, 1, 40, 6, 0, 0, 0, 0, 0f, 0f),
            new Ch(1, 1, 1, 1, 12, 12, 0, 0, 0, 0, 0f, 0f)});
        // 3: alignment — center (0.5) and right/bottom (1.0) in a wide/tall cell; tests the
        // (int) vs Math.round asymmetry on .5 lerps.
        emit(5, 7, 0, 0, new Ch[]{
            new Ch(0, 0, 1, 1, 40, 40, 0, 0, 0, 0, 0f, 0f),   // big cell defines col0/row0 size
            new Ch(0, 1, 1, 1, 10, 10, 0, 0, 0, 0, 0.5f, 0.5f),
            new Ch(1, 0, 1, 1, 11, 11, 0, 0, 0, 0, 1.0f, 1.0f)});
        // 4: padding on all sides + alignment.
        emit(0, 0, 1, 1, new Ch[]{
            new Ch(0, 0, 1, 1, 30, 30, 0, 0, 0, 0, 0f, 0f),
            new Ch(0, 1, 1, 1, 8, 8, 3, 2, 5, 4, 0.5f, 0.5f)});
        // 5: spans (a child occupying 2 columns / 2 rows) -> Divisor splits across spans.
        emit(0, 0, 2, 2, new Ch[]{
            new Ch(0, 0, 1, 2, 25, 10, 0, 0, 0, 0, 0f, 0f),   // spans 2 columns
            new Ch(1, 0, 2, 1, 10, 25, 0, 0, 0, 0, 0f, 0f),   // spans 2 rows
            new Ch(1, 1, 1, 1, 9, 9, 0, 0, 0, 0, 0f, 0f),
            new Ch(2, 1, 1, 1, 9, 9, 0, 0, 0, 0, 0f, 0f)});
        // 6: alignment 0.5 on ODD leftover (truncate vs round differ): cell 21 wide/tall, child 10.
        emit(0, 0, 0, 0, new Ch[]{
            new Ch(0, 0, 1, 1, 21, 21, 0, 0, 0, 0, 0f, 0f),
            new Ch(0, 1, 1, 1, 10, 10, 0, 0, 0, 0, 0.5f, 0.5f),
            new Ch(1, 0, 1, 1, 10, 10, 0, 0, 0, 0, 0.5f, 0.5f)});

        // --- FrameLayout (overlapping, aligned within resolved frame size) ---
        // F1: two children, frame size = max(child sizes); centered + corner alignments.
        emitFrame(10, 10, 0, 0, new Ch[]{
            new Ch(0, 0, 1, 1, 40, 30, 0, 0, 0, 0, 0f, 0f),       // defines frame size
            new Ch(0, 0, 1, 1, 10, 10, 0, 0, 0, 0, 0.5f, 0.5f),   // centered
            new Ch(0, 0, 1, 1, 12, 8, 0, 0, 0, 0, 1.0f, 1.0f)});  // bottom-right
        // F2: minWidth/minHeight larger than children; odd leftover (truncate vs round).
        emitFrame(0, 0, 25, 25, new Ch[]{
            new Ch(0, 0, 1, 1, 10, 10, 0, 0, 0, 0, 0.5f, 0.5f),
            new Ch(0, 0, 1, 1, 6, 6, 0, 0, 0, 0, 0f, 1.0f)});
        // F3: padding + alignment.
        emitFrame(3, 4, 0, 0, new Ch[]{
            new Ch(0, 0, 1, 1, 30, 30, 0, 0, 0, 0, 0f, 0f),
            new Ch(0, 0, 1, 1, 8, 8, 2, 3, 4, 1, 0.5f, 0.5f)});

        // --- LinearLayout (1D grid) ---
        // L1: horizontal row, varied heights -> row height = max; vertical-center align per child.
        emitLinear(true, 10, 5, 4, new Ch[]{
            new Ch(0, 0, 1, 1, 16, 12, 0, 0, 0, 0, 0f, 0.5f),
            new Ch(0, 0, 1, 1, 16, 20, 0, 0, 0, 0, 0f, 0.5f),
            new Ch(0, 0, 1, 1, 16, 8, 0, 0, 0, 0, 0f, 0.5f)});
        // L2: vertical column, varied widths -> column width = max; horizontal alignment.
        emitLinear(false, 0, 0, 3, new Ch[]{
            new Ch(0, 0, 1, 1, 10, 10, 0, 0, 0, 0, 0.5f, 0f),
            new Ch(0, 0, 1, 1, 24, 10, 0, 0, 0, 0, 1.0f, 0f),
            new Ch(0, 0, 1, 1, 14, 10, 0, 0, 0, 0, 0f, 0f)});
    }
}
