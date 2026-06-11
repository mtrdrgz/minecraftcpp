// Ground truth for the GUI layout POSITIONING math —
//   net.minecraft.client.gui.layouts.{FrameLayout, SpacerElement, LayoutSettings,
//   AbstractLayout.AbstractChildWrapper}
// Drives the REAL classes headless (layout is pure int/float math, no GL) and emits a
// TSV that mcpp/src/client/gui/layouts/FrameLayoutParityTest.cpp replays against the port.
//
// Float alignment inputs are emitted as raw IEEE bits (decimal int) so the C++ side
// reconstructs the identical float; outputs are widget/frame ints (bit-exact by nature).
//
//   FRAME   x y minW minH childW childH padL padT padR padB axBits ayBits  outX outY outW outH
//   ALIGND  pos length widgetLen alignBits  out
//   MULTI3  x y minW minH  w0 h0 w1 h1 w2 h2  axBits ayBits  x0 y0 x1 y1 x2 y2 outW outH
import net.minecraft.client.gui.layouts.FrameLayout;
import net.minecraft.client.gui.layouts.LayoutSettings;
import net.minecraft.client.gui.layouts.SpacerElement;

public class FrameLayoutParity {
    static final StringBuilder O = new StringBuilder();

    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        int[] coords = { -7, 0, 3, 100 };
        int[] mins = { 0, 5, 40 };
        int[] sizes = { 0, 1, 7, 20, 41 };
        int[] pads = { 0, 2, 5 };
        float[] aligns = { 0.0f, 0.5f, 1.0f, 0.25f, 0.75f, 0.33f, 0.1f };

        // (A) single-child FrameLayout over a representative cross-section.
        int seed = 0;
        for (int x : coords) for (int minW : mins) for (int cw : sizes) {
            // pick deterministic-but-varied partners so we don't blow up the matrix
            int y = coords[(seed) % coords.length];
            int minH = mins[(seed / 2) % mins.length];
            int ch = sizes[(seed / 3) % sizes.length];
            int padL = pads[(seed) % pads.length], padT = pads[(seed / 2) % pads.length];
            int padR = pads[(seed / 3) % pads.length], padB = pads[(seed / 5) % pads.length];
            float ax = aligns[(seed) % aligns.length], ay = aligns[(seed / 4) % aligns.length];
            seed++;
            emitFrame(x, y, minW, minH, cw, ch, padL, padT, padR, padB, ax, ay);
        }

        // (B) alignInDimension over edge offsets / alignments.
        for (int pos : new int[]{ -10, 0, 5, 200 })
            for (int len : new int[]{ 0, 1, 10, 33, 100 })
                for (int wl : new int[]{ 0, 1, 9, 33, 120 })
                    for (float a : aligns)
                        emitAlignDim(pos, len, wl, a);

        // (C) multi-child frame: result size = max over children (+ min).
        emitMulti3(3, -2, 10, 4, 8, 6, 20, 3, 5, 30, 0.5f, 0.5f);
        emitMulti3(0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 0.0f, 1.0f);
        emitMulti3(50, 50, 100, 10, 7, 7, 7, 40, 41, 7, 0.25f, 0.75f);

        System.out.print(O);
    }

    static void emitFrame(int x, int y, int minW, int minH, int cw, int ch,
                          int padL, int padT, int padR, int padB, float ax, float ay) {
        FrameLayout frame = new FrameLayout(x, y, minW, minH);
        SpacerElement child = new SpacerElement(cw, ch);
        LayoutSettings s = frame.newChildLayoutSettings().padding(padL, padT, padR, padB).align(ax, ay);
        frame.addChild(child, s);
        frame.arrangeElements();
        O.append("FRAME\t").append(x).append('\t').append(y).append('\t').append(minW).append('\t').append(minH)
         .append('\t').append(cw).append('\t').append(ch).append('\t').append(padL).append('\t').append(padT)
         .append('\t').append(padR).append('\t').append(padB).append('\t').append(Float.floatToRawIntBits(ax))
         .append('\t').append(Float.floatToRawIntBits(ay)).append('\t')
         .append(child.getX()).append('\t').append(child.getY()).append('\t')
         .append(frame.getWidth()).append('\t').append(frame.getHeight()).append('\n');
    }

    static void emitAlignDim(int pos, int len, int wl, float align) {
        int[] cap = new int[1];
        FrameLayout.alignInDimension(pos, len, wl, v -> cap[0] = v, align);
        O.append("ALIGND\t").append(pos).append('\t').append(len).append('\t').append(wl)
         .append('\t').append(Float.floatToRawIntBits(align)).append('\t').append(cap[0]).append('\n');
    }

    static void emitMulti3(int x, int y, int minW, int minH,
                           int w0, int h0, int w1, int h1, int w2, int h2, float ax, float ay) {
        FrameLayout frame = new FrameLayout(x, y, minW, minH);
        SpacerElement c0 = new SpacerElement(w0, h0), c1 = new SpacerElement(w1, h1), c2 = new SpacerElement(w2, h2);
        frame.addChild(c0, frame.newChildLayoutSettings().align(ax, ay));
        frame.addChild(c1, frame.newChildLayoutSettings().align(ax, ay));
        frame.addChild(c2, frame.newChildLayoutSettings().align(ax, ay));
        frame.arrangeElements();
        O.append("MULTI3\t").append(x).append('\t').append(y).append('\t').append(minW).append('\t').append(minH)
         .append('\t').append(w0).append('\t').append(h0).append('\t').append(w1).append('\t').append(h1)
         .append('\t').append(w2).append('\t').append(h2).append('\t').append(Float.floatToRawIntBits(ax))
         .append('\t').append(Float.floatToRawIntBits(ay)).append('\t')
         .append(c0.getX()).append('\t').append(c0.getY()).append('\t')
         .append(c1.getX()).append('\t').append(c1.getY()).append('\t')
         .append(c2.getX()).append('\t').append(c2.getY()).append('\t')
         .append(frame.getWidth()).append('\t').append(frame.getHeight()).append('\n');
    }
}
