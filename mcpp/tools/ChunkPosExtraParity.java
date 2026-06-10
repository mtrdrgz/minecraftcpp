// Ground-truth generator for net.minecraft.world.level.ChunkPos coordinate helpers.
// Emits tab-separated rows: <TAG> <inputs...> <outputs...> (all decimal ints).
//
// Every value comes from calling the REAL net.minecraft methods directly:
//   getMinBlockX/Z, getMaxBlockX/Z, getMiddleBlockX/Z, getBlockX/Z(offset),
//   getRegionX/Z, getRegionLocalX/Z, getChessboardDistance(int,int),
//   distanceSquared(int,int).
//
// distanceSquared(int,int) is a PRIVATE helper -> reflection.
// All others are public instance methods on the ChunkPos record.
//
// Compile/run against the same Minecraft jar used elsewhere in this repo.

import java.lang.reflect.Method;
import net.minecraft.world.level.ChunkPos;

public class ChunkPosExtraParity {
   static final java.io.PrintStream O = System.out;

   // Battery of coordinate values: zeros, small +/-, boundaries around the
   // <<4 / >>5 / &31 behaviour, and 32-bit extremes to exercise signed
   // wraparound exactly as Java's int does.
   static final int[] VALS = new int[] {
      0, 1, -1, 2, -2, 7, 8, 15, 16, -15, -16, -17, 31, 32, 33, -31, -32, -33,
      63, 64, -63, -64, 100, -100, 1000, -1000, 1875066, -1875066,
      65535, 65536, 131071, 131072,
      8388607, -8388608, 30000000, -30000000,
      0x0FFFFFFF, -0x0FFFFFFF,
      Integer.MAX_VALUE, Integer.MIN_VALUE,
      Integer.MAX_VALUE - 1, Integer.MIN_VALUE + 1
   };

   // Offsets for getBlockX/Z(offset): in-section 0..15 plus a few out-of-range
   // values to confirm it is plain (x<<4)+offset with no clamping.
   static final int[] OFFSETS = new int[] {
      0, 1, 7, 8, 14, 15, 16, -1, 100, -100,
      Integer.MAX_VALUE, Integer.MIN_VALUE
   };

   public static void main(String[] args) throws Exception {
      // ChunkPos.<clinit> pulls in ChunkStatus -> BuiltInRegistries (needs Bootstrap).
      // O is captured at class load (before this), so the TSV on stdout stays clean.
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Method distanceSquaredII =
         ChunkPos.class.getDeclaredMethod("distanceSquared", int.class, int.class);
      distanceSquaredII.setAccessible(true);

      // Single-coordinate-axis helpers. ChunkPos is symmetric in x/z, but we
      // exercise BOTH axes with independent x,z so the C++ side proves it reads
      // the correct field for each helper.
      for (int x : VALS) {
         for (int z : VALS) {
            ChunkPos p = new ChunkPos(x, z);

            // POS <x> <z> : the per-axis pure helpers.
            O.println("POS\t" + x + "\t" + z
               + "\t" + p.getMinBlockX()
               + "\t" + p.getMinBlockZ()
               + "\t" + p.getMaxBlockX()
               + "\t" + p.getMaxBlockZ()
               + "\t" + p.getMiddleBlockX()
               + "\t" + p.getMiddleBlockZ()
               + "\t" + p.getRegionX()
               + "\t" + p.getRegionZ()
               + "\t" + p.getRegionLocalX()
               + "\t" + p.getRegionLocalZ());
         }
      }

      // getBlockX/Z(offset): BLK <x> <z> <offset> <getBlockX> <getBlockZ>
      for (int x : VALS) {
         for (int z : VALS) {
            ChunkPos p = new ChunkPos(x, z);
            for (int off : OFFSETS) {
               O.println("BLK\t" + x + "\t" + z + "\t" + off
                  + "\t" + p.getBlockX(off)
                  + "\t" + p.getBlockZ(off));
            }
         }
      }

      // getChessboardDistance(int,int) and distanceSquared(int,int):
      // DIST <thisX> <thisZ> <argX> <argZ> <chessboard> <distSq>
      for (int tx : VALS) {
         for (int tz : VALS) {
            ChunkPos p = new ChunkPos(tx, tz);
            for (int ax : VALS) {
               for (int az : VALS) {
                  int cd = p.getChessboardDistance(ax, az);
                  int dsq = (Integer) distanceSquaredII.invoke(p, ax, az);
                  O.println("DIST\t" + tx + "\t" + tz + "\t" + ax + "\t" + az
                     + "\t" + cd + "\t" + dsq);
               }
            }
         }
      }
   }
}
