// Ground-truth generator for the position long-packing codecs using the REAL
// decompiled 26.1.2 classes (net.minecraft.core.BlockPos / SectionPos,
// net.minecraft.world.level.ChunkPos). Pure bit arithmetic, no Bootstrap.
//
//   tools/run_groundtruth.ps1 -Tool PosCodecParity -Out mcpp/build/pos_codec.tsv
//
// Longs/shorts/ints emitted in decimal; the C++ test recomputes and must match.

import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;

public class PosCodecParity {
    static final java.io.PrintStream O = System.out;

    static final int[] XS = { 0, 1, -1, 15, 16, -16, 17, 100, -100, 1000, -1000,
        33554431, -33554432, 33554430, -33554431, 30000000, -30000000, 12345, -67890, 2097151, -2097152 };
    static final int[] YS = { 0, 1, -1, 15, 16, -16, 63, 64, -64, 255, 256, -320, 2047, -2048, 2046, -2047, 1000, -1000, 319, 384 };

    public static void main(String[] args) {
        // ChunkPos.<clinit> pulls in ChunkStatus -> BuiltInRegistries, which needs
        // Bootstrap. O was captured at class load (before this), so the TSV on
        // stdout stays clean even after log4j reconfigures System.out.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // BlockPos pack/unpack/offset/flatIndex
        for (int x : XS) for (int y : YS) for (int z : XS) {
            long node = BlockPos.asLong(x, y, z);
            O.println("BP_ASLONG\t" + x + "\t" + y + "\t" + z + "\t" + node);
            O.println("BP_GET\t" + node + "\t" + BlockPos.getX(node) + "\t" + BlockPos.getY(node) + "\t" + BlockPos.getZ(node));
            O.println("BP_FLAT\t" + node + "\t" + BlockPos.getFlatIndex(node));
        }
        // offsets
        int[] STEPS = { 0, 1, -1, 15, -15, 16 };
        long base = BlockPos.asLong(100, 64, -200);
        for (int sx : STEPS) for (int sy : STEPS) for (int sz : STEPS)
            O.println("BP_OFFSET\t" + base + "\t" + sx + "\t" + sy + "\t" + sz + "\t" + BlockPos.offset(base, sx, sy, sz));

        // ChunkPos pack/unpack
        for (int x : XS) for (int z : XS) {
            long key = ChunkPos.pack(x, z);
            O.println("CP_ASLONG\t" + x + "\t" + z + "\t" + key);
            O.println("CP_GET\t" + key + "\t" + ChunkPos.getX(key) + "\t" + ChunkPos.getZ(key));
        }

        // SectionPos pack/unpack
        int[] SXS = { 0, 1, -1, 15, -16, 100, -100, 2097151, -2097152, 2097150, -2097151, 12345, -54321 };
        int[] SYS = { 0, 1, -1, 15, -16, 31, -32, 524287, -524288, 524286, -524287, 200, -200 };
        for (int x : SXS) for (int y : SYS) for (int z : SXS) {
            long node = SectionPos.asLong(x, y, z);
            O.println("SP_ASLONG\t" + x + "\t" + y + "\t" + z + "\t" + node);
            O.println("SP_GET\t" + node + "\t" + SectionPos.x(node) + "\t" + SectionPos.y(node) + "\t" + SectionPos.z(node));
        }
        // blockToSectionCoord / sectionToBlockCoord
        for (int b : XS) {
            O.println("B2S\t" + b + "\t" + SectionPos.blockToSectionCoord(b));
            O.println("S2B\t" + b + "\t" + SectionPos.sectionToBlockCoord(b));
            for (int off : new int[]{0, 7, 15})
                O.println("S2B_OFF\t" + b + "\t" + off + "\t" + SectionPos.sectionToBlockCoord(b, off));
        }
        for (double d : new double[]{ 0.0, 0.5, 15.9, 16.0, -0.1, -16.1, 100.7, -100.7, 31.999, -32.0 })
            O.println("B2S_D\t" + Double.doubleToRawLongBits(d) + "\t" + SectionPos.blockToSectionCoord(d));

        // sectionRelativePos round-trip
        for (int x : new int[]{0, 1, 7, 15, 16, -1, -16, 100, -100})
            for (int y : new int[]{0, 5, 15, 16, -1, -16})
                for (int z : new int[]{0, 3, 15, 16, -1, -100}) {
                    short rel = SectionPos.sectionRelativePos(new BlockPos(x, y, z));
                    O.println("SP_REL\t" + x + "\t" + y + "\t" + z + "\t" + rel
                        + "\t" + SectionPos.sectionRelativeX(rel)
                        + "\t" + SectionPos.sectionRelativeY(rel)
                        + "\t" + SectionPos.sectionRelativeZ(rel));
                }
    }
}
