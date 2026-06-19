import net.minecraft.core.Direction;

// Ground-truth emitter for the 2D / yaw helpers of net.minecraft.core.Direction
// (Minecraft 26.1.2). Emits tab-separated rows consumed by Direction2DParityTest.cpp.
//
// Direction ordinals (Direction.java:33-38): DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
// All ints/booleans as decimal; floats as raw int bits via Float.floatToRawIntBits.
public class Direction2DParity {
   static final java.io.PrintStream O = System.out;

   // Map a Direction to the C++ ordinal (DOWN=0..EAST=5). Java enum ordinal()
   // already matches that declaration order exactly.
   static int ord(Direction d) { return d.ordinal(); }

   static final Direction[] ALL = { Direction.DOWN, Direction.UP, Direction.NORTH,
                                    Direction.SOUTH, Direction.WEST, Direction.EAST };
   static final Direction[] HORIZ = { Direction.SOUTH, Direction.WEST, Direction.NORTH, Direction.EAST };

   public static void main(String[] args) throws Exception {
      // GET2D <ord> <get2DDataValue>   (all 6 directions; vertical ones return -1)
      for (Direction d : ALL) {
         O.println("GET2D\t" + ord(d) + "\t" + d.get2DDataValue());
      }

      // TOYROT <ord> <toYRot bits>      (all 6 directions; toYRot is data2d&3 * 90)
      for (Direction d : ALL) {
         O.println("TOYROT\t" + ord(d) + "\t" + String.format("%08x", Float.floatToRawIntBits(d.toYRot())));
      }

      // GETYROT <ord> <getYRot bits>    (horizontal only — vertical throws)
      for (Direction d : HORIZ) {
         O.println("GETYROT\t" + ord(d) + "\t" + String.format("%08x", Float.floatToRawIntBits(Direction.getYRot(d))));
      }

      // FROM2D <data> <ord>             (from2DDataValue: thorough int battery incl. negatives/edges)
      int[] data2dCases = {
         -1000000, -100003, -65537, -65536, -1024, -257, -256, -255, -129, -128, -127,
         -100, -64, -33, -32, -31, -17, -16, -15, -9, -8, -7, -5, -4, -3, -2, -1, 0,
         1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 255,
         256, 257, 1023, 1024, 65535, 65536, 65537, 100003, 1000000, 2000000001,
         Integer.MAX_VALUE, Integer.MIN_VALUE + 4
      };
      for (int d : data2dCases) {
         O.println("FROM2D\t" + d + "\t" + ord(Direction.from2DDataValue(d)));
      }

      // FROMYROT <yRot bits> <ord>      (fromYRot: dense + wrap + negatives + boundaries)
      double[] yRotCases = {
         -720.0, -540.0, -451.0, -450.0, -449.0, -405.0, -360.0, -315.0, -271.0, -270.0,
         -269.0, -226.0, -225.0, -224.0, -181.0, -180.0, -179.0, -136.0, -135.0, -134.0,
         -91.0, -90.0, -89.0, -46.0, -45.0, -44.0, -1.0, -0.5, -0.0, 0.0, 0.5, 1.0,
         44.0, 44.9, 45.0, 45.1, 46.0, 89.0, 90.0, 91.0, 134.0, 135.0, 136.0, 179.0,
         180.0, 181.0, 224.0, 225.0, 226.0, 269.0, 270.0, 271.0, 314.0, 315.0, 316.0,
         359.0, 360.0, 361.0, 404.0, 405.0, 449.0, 450.0, 451.0, 540.0, 719.0, 720.0,
         1234.5, -1234.5, 22.5, 67.5, 112.5, 157.5, 202.5, 247.5, 292.5, 337.5,
         0.49999999, -0.49999999, 89.99999, 90.00001
      };
      for (double y : yRotCases) {
         O.println("FROMYROT\t" + String.format("%016x", Double.doubleToRawLongBits(y)) + "\t" + ord(Direction.fromYRot(y)));
      }

      // OPP2D <ord> <opposite ord>      (horizontal opposite via getOpposite)
      for (Direction d : HORIZ) {
         O.println("OPP2D\t" + ord(d) + "\t" + ord(d.getOpposite()));
      }
   }
}
