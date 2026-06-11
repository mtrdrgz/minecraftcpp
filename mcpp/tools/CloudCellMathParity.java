// Ground-truth generator for the pure cell/geometry math inside the REAL
// net.minecraft.client.renderer.CloudRenderer (Minecraft 26.1.2).
//
// CloudRenderer's constructor allocates GPU ring buffers, so we NEVER call it. We
// reach the pure private static helpers (packCellData, isCellEmpty,
// isNorth/East/South/WestEmpty, getSizeForCloudDistance) by reflection, and the
// pure private instance method encodeFace(ByteBuffer,int,int,Direction,int) on an
// instance allocated via sun.misc.Unsafe.allocateInstance (NO constructor runs, so
// no GL device is touched — encodeFace reads no instance fields). The per-frame
// cloud-cell positioning math lives inline inside render(); we reproduce those exact
// statements here verbatim from CloudRenderer.java:181-191 (identical literals and
// float/double narrowing — the same arithmetic the real method executes).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool CloudCellMathParity -Out mcpp/build/cloud_cell_math.tsv
//
// TSV rows (tab-separated), dispatched by leading TAG in the C++ test:
//   PACK    <color> <n> <e> <s> <w> <packedLong>   (n/e/s/w in {0,1})
//   EMPTY   <color> <isEmpty:0|1>
//   SIDES   <cellData> <north> <east> <south> <west>   (bits in {0,1})
//   SIZE    <radiusCells> <int32>
//   FACE    <dir3d> <flags> <x> <z> <bx> <bz> <bDirFlags>   (bytes as signed ints)
//   POS     <width> <height> <camXbits> <camZbits> <gameTime> <ptBits>
//           <cloudXbits> <cloudZbits> <cellX> <cellZ> <xInCellBits> <zInCellBits>
// Floats/doubles are emitted as raw bit patterns (Float.floatToRawIntBits /
// Double.doubleToRawLongBits) so the C++ side compares bit-for-bit.

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;

public class CloudCellMathParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Defensive bootstrap (CloudRenderer's pure helpers need none, but Direction
        // class-init and Identifier-free paths are fine; keep parity-harness uniform).
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — not required for these pure helpers.
        }

        Class<?> CR = Class.forName("net.minecraft.client.renderer.CloudRenderer");

        Method mPack = CR.getDeclaredMethod("packCellData", int.class, boolean.class,
                boolean.class, boolean.class, boolean.class);
        Method mEmpty = CR.getDeclaredMethod("isCellEmpty", int.class);
        Method mNorth = CR.getDeclaredMethod("isNorthEmpty", long.class);
        Method mEast = CR.getDeclaredMethod("isEastEmpty", long.class);
        Method mSouth = CR.getDeclaredMethod("isSouthEmpty", long.class);
        Method mWest = CR.getDeclaredMethod("isWestEmpty", long.class);
        Method mSize = CR.getDeclaredMethod("getSizeForCloudDistance", int.class);
        Method mEncode = CR.getDeclaredMethod("encodeFace", ByteBuffer.class, int.class,
                int.class, net.minecraft.core.Direction.class, int.class);
        for (Method m : new Method[]{mPack, mEmpty, mNorth, mEast, mSouth, mWest, mSize, mEncode}) {
            m.setAccessible(true);
        }

        // GL-free CloudRenderer instance: allocate WITHOUT running the constructor.
        Field uf = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        uf.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) uf.get(null);
        Object cr = unsafe.allocateInstance(CR);

        // ── PACK + EMPTY + SIDES ────────────────────────────────────────────
        int[] colors = {
            0x00000000, 0xFF000000, 0x0AFFFFFF, 0x09FFFFFF, 0xFFFFFFFF, 0x80345678,
            0x0BAADF00, 0xCAFEBABE, 0x12345678, 0xFEDCBA98, 0x00112233, 0xABCDEF01,
            0x7FFFFFFF, 0x80000000, 0xFF7F00FF, 0x0000000A, 0x00000009,
        };
        boolean[] bools = {false, true};
        for (int color : colors) {
            boolean empty = (boolean) mEmpty.invoke(null, color);
            O.println("EMPTY\t" + color + "\t" + (empty ? 1 : 0));
            for (boolean n : bools) for (boolean e : bools) for (boolean s : bools) for (boolean w : bools) {
                long packed = (long) mPack.invoke(null, color, n, e, s, w);
                O.println("PACK\t" + color + "\t" + (n ? 1 : 0) + "\t" + (e ? 1 : 0)
                        + "\t" + (s ? 1 : 0) + "\t" + (w ? 1 : 0) + "\t" + packed);
                boolean bn = (boolean) mNorth.invoke(null, packed);
                boolean be = (boolean) mEast.invoke(null, packed);
                boolean bs = (boolean) mSouth.invoke(null, packed);
                boolean bw = (boolean) mWest.invoke(null, packed);
                O.println("SIDES\t" + packed + "\t" + (bn ? 1 : 0) + "\t" + (be ? 1 : 0)
                        + "\t" + (bs ? 1 : 0) + "\t" + (bw ? 1 : 0));
            }
        }
        // A few hand-built cellData values (exercise the side bits directly).
        long[] cellDatas = {0L, 1L, 2L, 4L, 8L, 15L, 0xFFFFFFFF0L, 0x80000000L << 4, -1L};
        for (long cd : cellDatas) {
            boolean bn = (boolean) mNorth.invoke(null, cd);
            boolean be = (boolean) mEast.invoke(null, cd);
            boolean bs = (boolean) mSouth.invoke(null, cd);
            boolean bw = (boolean) mWest.invoke(null, cd);
            O.println("SIDES\t" + cd + "\t" + (bn ? 1 : 0) + "\t" + (be ? 1 : 0)
                    + "\t" + (bs ? 1 : 0) + "\t" + (bw ? 1 : 0));
        }

        // ── SIZE ────────────────────────────────────────────────────────────
        int[] radii = {0, 1, 2, 3, 4, 5, 8, 11, 16, 21, 32, 43, 64, 100, 171, 256};
        for (int r : radii) {
            int sz = (int) mSize.invoke(null, r);
            O.println("SIZE\t" + r + "\t" + sz);
        }

        // ── FACE (encodeFace byte packing) ──────────────────────────────────
        net.minecraft.core.Direction[] dirs = net.minecraft.core.Direction.values();
        int[] flagsList = {0, 16, 32, 16 | 32};
        int[] xs = {-43, -8, -3, -2, -1, 0, 1, 2, 3, 7, 8, 42, 127, 128, 255, -128, -129};
        int[] zs = {-42, -7, -1, 0, 1, 5, 8, 127, -128, 254, -255};
        for (net.minecraft.core.Direction d : dirs) {
            for (int flags : flagsList) {
                for (int x : xs) for (int z : zs) {
                    ByteBuffer buf = ByteBuffer.allocate(3);
                    mEncode.invoke(cr, buf, x, z, d, flags);
                    buf.flip();
                    int bx = buf.get();   // signed byte -> int
                    int bz = buf.get();
                    int bdf = buf.get();
                    O.println("FACE\t" + d.get3DDataValue() + "\t" + flags + "\t" + x
                            + "\t" + z + "\t" + bx + "\t" + bz + "\t" + bdf);
                }
            }
        }

        // ── POS (render() per-frame cloud-cell positioning, lines 181-191) ──
        // Reproduced verbatim from CloudRenderer.render() — identical literals and
        // float/double narrowing as the real method executes (it is inline, so there
        // is no method to reflect; this IS the same arithmetic, copied exactly).
        int[] widths = {12, 16, 32, 64, 256};
        int[] heights = {12, 16, 32, 64, 256};
        double[] cams = {0.0, 0.5, -0.5, 12.3, -12.3, 1234.56, -987.65, 1.0e6, -1.0e6, 3.96};
        long[] gameTimes = {0L, 1L, 199L, 200L, 400L, 4799L, 6399L, 123456L, 1000000L, 24000L};
        float[] pts = {0.0F, 0.25F, 0.5F, 0.75F, 1.0F, 0.333333F};
        int ci = 0;
        for (int width : widths) {
            int height = heights[ci % heights.length];
            ci++;
            for (double camX : cams) {
                double camZ = cams[(int) (camX == 0 ? 1 : 2) % cams.length]; // mild variety
                for (long gameTime : gameTimes) {
                    for (float partialTicks : pts) {
                        // --- verbatim render() math ---
                        float cloudOffset = (float) (gameTime % (width * 400L)) + partialTicks;
                        double cloudX = camX + cloudOffset * 0.030000001F;
                        double cloudZ = camZ + 3.96F;
                        double textureWidthBlocks = width * 12.0;
                        double textureHeightBlocks = height * 12.0;
                        cloudX -= net.minecraft.util.Mth.floor(cloudX / textureWidthBlocks) * textureWidthBlocks;
                        cloudZ -= net.minecraft.util.Mth.floor(cloudZ / textureHeightBlocks) * textureHeightBlocks;
                        int cellX = net.minecraft.util.Mth.floor(cloudX / 12.0);
                        int cellZ = net.minecraft.util.Mth.floor(cloudZ / 12.0);
                        float xInCell = (float) (cloudX - cellX * 12.0F);
                        float zInCell = (float) (cloudZ - cellZ * 12.0F);
                        // --- end verbatim ---
                        O.println("POS\t" + width + "\t" + height
                                + "\t" + Double.doubleToRawLongBits(camX)
                                + "\t" + Double.doubleToRawLongBits(camZ)
                                + "\t" + gameTime
                                + "\t" + Float.floatToRawIntBits(partialTicks)
                                + "\t" + Double.doubleToRawLongBits(cloudX)
                                + "\t" + Double.doubleToRawLongBits(cloudZ)
                                + "\t" + cellX + "\t" + cellZ
                                + "\t" + Float.floatToRawIntBits(xInCell)
                                + "\t" + Float.floatToRawIntBits(zInCell));
                    }
                }
            }
        }
    }
}
