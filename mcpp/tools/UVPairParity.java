// Ground truth for net.minecraft.client.model.geom.builders.UVPair.pack/unpackU/unpackV
// (the packed-UV stored per BakedQuad vertex). Pure bit math; drives the REAL class.
//
//   tools/run_groundtruth.ps1 -Tool UVPairParity -Out mcpp/build/uvpair.tsv
//
// Row: PACK \t <uBits %08x> \t <vBits %08x> \t <packed %016x>
//      UNPK \t <packed %016x> \t <unpackUBits %08x> \t <unpackVBits %08x>

import net.minecraft.client.model.geom.builders.UVPair;

public class UVPairParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }
    static String l(long x)  { return String.format("%016x", x); }

    public static void main(String[] args) throws Exception {
        // Finite/physical UV floats: atlas coords in [0,1], the [0,16] model range, exact
        // halves, and a few magnitudes/signs (the packer is total over finite floats).
        float[] vals = {
            0.0f, 1.0f, -1.0f, 0.5f, 0.25f, 0.0625f, 0.99999994f, 0.00006103515625f,
            16.0f, 8.0f, 15.5f, 0.03125f, 0.1f, 0.2f, 0.7f, 123.456f, -0.5f, 2.0f
        };
        for (float u : vals) {
            for (float v : vals) {
                long p = UVPair.pack(u, v);
                O.println("PACK\t" + f(u) + "\t" + f(v) + "\t" + l(p));
                O.println("UNPK\t" + l(p) + "\t" + f(UVPair.unpackU(p)) + "\t" + f(UVPair.unpackV(p)));
            }
        }
    }
}
