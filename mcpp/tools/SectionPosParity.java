// Ground-truth generator for net.minecraft.core.SectionPos using the REAL
// decompiled 26.1.2 class. Covers the long bit-packing (asLong / x / y / z),
// blockToSectionCoord / sectionToBlockCoord (+ offset overload), and the
// relative/offset surface (offset(long,int,int,int), offset(long,Direction),
// and the instance offset(int,int,int)).
//
//   tools/run_groundtruth.ps1 -Tool SectionPosParity -Out mcpp/build/section_pos.tsv
//
// All values are pure bit arithmetic; longs/ints emitted in decimal. The C++ test
// (mcpp/src/core/SectionPosParityTest.cpp) recomputes and compares bit-for-bit.
//
// SectionPos.offset(...) and the instance ctor go through reflection because the
// constructor is private; the static offset methods are public but we exercise the
// instance path too via the public SectionPos.of(long) factory.

import java.lang.reflect.Method;
import net.minecraft.core.Direction;
import net.minecraft.core.SectionPos;

public class SectionPosParity {
    static final java.io.PrintStream O = System.out;

    // Section coordinates: in-range and at/over the 22-bit (X/Z, +/-2^21) and
    // 20-bit (Y, +/-2^19) field edges so packing wrap is exercised.
    static final int[] SXS = { 0, 1, -1, 15, -16, 100, -100, 2097151, -2097152, 2097150, -2097151, 12345, -54321 };
    static final int[] SYS = { 0, 1, -1, 15, -16, 31, -32, 524287, -524288, 524286, -524287, 200, -200 };
    // Block coordinates (for block<->section conversion). Includes world horizontal
    // edges and Y build-range edges.
    static final int[] BXS = { 0, 1, -1, 15, 16, -16, 17, 100, -100, 1000, -1000,
        30000000, -30000000, 12345, -67890, 33554431, -33554432 };
    static final int[] STEPS = { 0, 1, -1, 15, -15, 16, -16, 100, -100 };

    public static void main(String[] args) throws Exception {
        // SectionPos itself is pure, but Direction.<clinit> / the STREAM_CODEC static
        // pulls in registry-ish classes on some builds; bootstrap to be safe. O was
        // captured at class load so the TSV on stdout stays clean.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Reflection handles for the static offset overloads + the instance offset
        // (private ctor). of(long) and the static offsets are public, but resolving
        // them reflectively keeps the tool robust to access changes.
        Method mOffXYZ = SectionPos.class.getDeclaredMethod("offset", long.class, int.class, int.class, int.class);
        mOffXYZ.setAccessible(true);
        Method mOffDir = SectionPos.class.getDeclaredMethod("offset", long.class, Direction.class);
        mOffDir.setAccessible(true);
        Method mInstOff = SectionPos.class.getDeclaredMethod("offset", int.class, int.class, int.class);
        mInstOff.setAccessible(true);

        // --- asLong pack + x/y/z unpack ---------------------------------------
        for (int x : SXS) for (int y : SYS) for (int z : SXS) {
            long node = SectionPos.asLong(x, y, z);
            O.println("SP_ASLONG\t" + x + "\t" + y + "\t" + z + "\t" + node);
            O.println("SP_GET\t" + node + "\t" + SectionPos.x(node) + "\t" + SectionPos.y(node) + "\t" + SectionPos.z(node));
        }

        // --- block <-> section coordinate conversion --------------------------
        for (int b : BXS) {
            O.println("B2S\t" + b + "\t" + SectionPos.blockToSectionCoord(b));
            O.println("S2B\t" + b + "\t" + SectionPos.sectionToBlockCoord(b));
            for (int off : new int[]{ 0, 1, 7, 8, 15 })
                O.println("S2B_OFF\t" + b + "\t" + off + "\t" + SectionPos.sectionToBlockCoord(b, off));
        }

        // --- static offset(long, stepX, stepY, stepZ) -------------------------
        long[] bases = {
            SectionPos.asLong(0, 0, 0),
            SectionPos.asLong(100, 5, -200),
            SectionPos.asLong(-1, -1, -1),
            SectionPos.asLong(2097151, 524287, -2097152),
            SectionPos.asLong(12345, 200, -54321),
        };
        for (long base : bases) {
            for (int sx : STEPS) for (int sy : STEPS) for (int sz : STEPS) {
                long got = (Long) mOffXYZ.invoke(null, base, sx, sy, sz);
                O.println("SP_OFFSET\t" + base + "\t" + sx + "\t" + sy + "\t" + sz + "\t" + got);
            }
        }

        // --- static offset(long, Direction) -----------------------------------
        for (long base : bases) {
            for (Direction d : Direction.values()) {
                long got = (Long) mOffDir.invoke(null, base, d);
                O.println("SP_OFFSET_DIR\t" + base + "\t" + d.ordinal() + "\t" + got);
            }
        }

        // --- instance offset(int,int,int): emit the resulting section coords ---
        // Build via the public SectionPos.of(long) factory, then call the private
        // instance offset; read back x()/y()/z() of the returned SectionPos.
        for (long base : bases) {
            SectionPos sp = SectionPos.of(base);
            for (int sx : STEPS) for (int sy : STEPS) for (int sz : STEPS) {
                SectionPos r = (SectionPos) mInstOff.invoke(sp, sx, sy, sz);
                O.println("SP_INST_OFFSET\t" + sp.x() + "\t" + sp.y() + "\t" + sp.z()
                    + "\t" + sx + "\t" + sy + "\t" + sz
                    + "\t" + r.x() + "\t" + r.y() + "\t" + r.z());
            }
        }
    }
}
