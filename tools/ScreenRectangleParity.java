// Ground truth for net.minecraft.client.gui.navigation.ScreenRectangle (GUI rectangle geometry).
// Drives the REAL record's bounds + overlaps/intersects/encompasses/intersection/containsPoint.
//
//   tools/run_groundtruth.ps1 -Tool ScreenRectangleParity -Out mcpp/build/screen_rectangle.tsv
//
// Rows:
//   RECT <x> <y> <w> <h>  <top> <bottom> <left> <right>
//   PAIR <ax ay aw ah> <bx by bw bh>  <overlaps> <intersects> <encompasses> <isectValid> <ix iy iw ih>
//   PT   <x y w h> <px> <py> <contains>

import net.minecraft.client.gui.navigation.ScreenRectangle;

public class ScreenRectangleParity {
    static final java.io.PrintStream O = System.out;
    static int b(boolean x) { return x ? 1 : 0; }

    static final int[][] RECTS = {
        {0, 0, 10, 10}, {5, 5, 10, 10}, {10, 0, 5, 20}, {-3, -3, 6, 6}, {0, 0, 1, 1},
        {20, 20, 4, 4}, {2, 2, 6, 6}, {0, 0, 100, 50}, {9, 9, 2, 2}, {10, 10, 10, 10}
    };

    public static void main(String[] args) {
        for (int[] r : RECTS) {
            ScreenRectangle a = new ScreenRectangle(r[0], r[1], r[2], r[3]);
            O.println("RECT\t" + r[0] + "\t" + r[1] + "\t" + r[2] + "\t" + r[3]
                    + "\t" + a.top() + "\t" + a.bottom() + "\t" + a.left() + "\t" + a.right());
        }

        for (int[] ra : RECTS) {
            for (int[] rb : RECTS) {
                ScreenRectangle a = new ScreenRectangle(ra[0], ra[1], ra[2], ra[3]);
                ScreenRectangle bb = new ScreenRectangle(rb[0], rb[1], rb[2], rb[3]);
                ScreenRectangle isect = a.intersection(bb);
                StringBuilder sb = new StringBuilder("PAIR");
                for (int v : ra) sb.append('\t').append(v);
                for (int v : rb) sb.append('\t').append(v);
                sb.append('\t').append(b(a.overlaps(bb))).append('\t').append(b(a.intersects(bb)))
                  .append('\t').append(b(a.encompasses(bb)));
                if (isect == null) sb.append("\t0\t0\t0\t0\t0");
                else sb.append("\t1\t").append(isect.left()).append('\t').append(isect.top())
                       .append('\t').append(isect.width()).append('\t').append(isect.height());
                O.println(sb);
            }
        }

        int[][] pts = {{0, 0}, {9, 9}, {10, 10}, {5, 5}, {-1, -1}, {99, 49}, {100, 50}, {0, 10}};
        for (int[] r : RECTS) {
            ScreenRectangle a = new ScreenRectangle(r[0], r[1], r[2], r[3]);
            for (int[] pt : pts)
                O.println("PT\t" + r[0] + "\t" + r[1] + "\t" + r[2] + "\t" + r[3]
                        + "\t" + pt[0] + "\t" + pt[1] + "\t" + b(a.containsPoint(pt[0], pt[1])));
        }
    }
}
