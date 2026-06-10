// Ground truth for net.minecraft.client.renderer.texture.Stitcher (the atlas bin-packer).
// Drives the REAL Stitcher<Entry>: registerSprite -> stitch -> gatherSprites, dumping the
// final atlas extent (getWidth/getHeight) and each sprite's assigned (x,y) origin. Pure int
// math; no GL. The placements fully determine every sprite's atlas u0/u1/v0/v1.
//
//   tools/run_groundtruth.ps1 -Tool StitcherParity -Out mcpp/build/stitcher.tsv
//
// Per case (tab-separated, ints):
//   CASE  maxW maxH mipLevel anisotropyBit
//   SP  ns path width height          # registration order
//   RES  atlasW atlasH                # getWidth()/getHeight() after stitch
//   PL  ns path x y                   # gatherSprites placement (compared by name, order-free)
//   END

import java.util.ArrayList;
import java.util.List;
import net.minecraft.client.renderer.texture.Stitcher;
import net.minecraft.resources.Identifier;

public class StitcherParity {
    static final java.io.PrintStream O = System.out;

    record Spr(String ns, String path, int w, int h) {}

    static final class E implements Stitcher.Entry {
        final Spr s;
        final Identifier id;
        E(Spr s) { this.s = s; this.id = Identifier.fromNamespaceAndPath(s.ns(), s.path()); }
        public int width() { return s.w(); }
        public int height() { return s.h(); }
        public Identifier name() { return id; }
    }

    static void emit(int maxW, int maxH, int mip, int aniso, List<Spr> sprites) {
        O.println("CASE\t" + maxW + "\t" + maxH + "\t" + mip + "\t" + aniso);
        Stitcher<E> st = new Stitcher<>(maxW, maxH, mip, aniso);
        for (Spr s : sprites) {
            O.println("SP\t" + s.ns() + "\t" + s.path() + "\t" + s.w() + "\t" + s.h());
            st.registerSprite(new E(s));
        }
        boolean ok = true;
        try {
            st.stitch();
        } catch (Throwable t) {
            ok = false;  // StitcherException: doesn't fit. We choose cases that fit; mark RES -1 -1.
        }
        if (!ok) {
            O.println("RES\t-1\t-1");
            O.println("END");
            return;
        }
        O.println("RES\t" + st.getWidth() + "\t" + st.getHeight());
        List<String> placements = new ArrayList<>();
        st.gatherSprites((entry, x, z, padding) ->
            placements.add("PL\t" + entry.name().getNamespace() + "\t" + entry.name().getPath() + "\t" + x + "\t" + z));
        for (String p : placements) O.println(p);
        O.println("END");
    }

    static Spr sp(String path, int w, int h) { return new Spr("minecraft", path, w, h); }
    static Spr sp(String ns, String path, int w, int h) { return new Spr(ns, path, w, h); }

    public static void main(String[] args) throws Exception {
        // 1: uniform 16x16 grid (the common block-atlas case), mip0/aniso0.
        emit(512, 512, 0, 0, List.of(
            sp("a", 16, 16), sp("b", 16, 16), sp("c", 16, 16), sp("d", 16, 16),
            sp("e", 16, 16), sp("f", 16, 16), sp("g", 16, 16)));
        // 2: mixed sizes -> region splitting, mip0/aniso0.
        emit(512, 512, 0, 0, List.of(
            sp("big", 32, 32), sp("wide", 32, 16), sp("tall", 16, 32),
            sp("s1", 16, 16), sp("s2", 16, 16), sp("tiny", 8, 8), sp("tiny2", 8, 8)));
        // 3: mip2 (padding=4, rounding to mult of 4), mixed.
        emit(1024, 1024, 2, 0, List.of(
            sp("p1", 16, 16), sp("p2", 24, 24), sp("p3", 16, 32),
            sp("p4", 40, 8), sp("p5", 16, 16)));
        // 4: mip0 with anisotropy bits (padding grows).
        emit(2048, 2048, 0, 3, List.of(
            sp("q1", 16, 16), sp("q2", 16, 16), sp("q3", 32, 32), sp("q4", 16, 16)));
        // 5: many uniform -> exercises expansion choices (grow X vs Y).
        {
            List<Spr> many = new ArrayList<>();
            for (int i = 0; i < 20; i++) many.add(sp("u" + (char) ('a' + i), 16, 16));
            emit(256, 256, 0, 0, many);
        }
        // 6: non-power-of-two sizes, mip0.
        emit(512, 512, 0, 0, List.of(
            sp("n1", 14, 14), sp("n2", 14, 14), sp("n3", 30, 14), sp("n4", 14, 30), sp("n5", 6, 6)));
        // 7: namespace tie-break — identical w/h AND identical path, differing namespace.
        emit(512, 512, 0, 0, List.of(
            sp("aaa", "same", 16, 16), sp("bbb", "same", 16, 16), sp("ccc", "same", 16, 16)));
        // 8: path tie-break ordering — identical w/h, distinct paths in non-sorted reg order.
        emit(512, 512, 0, 0, List.of(
            sp("z", 16, 16), sp("m", 16, 16), sp("a", 16, 16), sp("q", 16, 16)));
        // 9: a single large sprite (atlas == that sprite, power-of-two rounded).
        emit(256, 256, 0, 0, List.of(sp("solo", 100, 60)));
        // 10: tall + wide mix forcing both growth directions, mip1.
        emit(1024, 1024, 1, 1, List.of(
            sp("t1", 64, 16), sp("t2", 16, 64), sp("t3", 32, 32), sp("t4", 16, 16),
            sp("t5", 48, 24), sp("t6", 24, 48)));
        // 11: maxWidth limited -> forces vertical growth.
        emit(64, 1024, 0, 0, List.of(
            sp("v1", 32, 16), sp("v2", 32, 16), sp("v3", 32, 16), sp("v4", 32, 16), sp("v5", 16, 16)));
    }
}
