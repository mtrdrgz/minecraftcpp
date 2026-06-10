import net.minecraft.core.Direction;
import java.util.Iterator;
import java.util.List;
import java.util.ArrayList;

// Ground-truth emitter for the requested subset of net.minecraft.core.Direction
// (Minecraft 26.1.2). Verifies the ALREADY-CERTIFIED C++ ports living in
// core/Direction2D.h (2D-data / yRot / opposite) and core/DirectionAxisExtra.h
// (getAxisDirection + Direction.Plane iteration order). No new header is written;
// this gate re-confirms those ports against the real class.
//
// Methods covered (verbatim names from Direction.java):
//   from2DDataValue(int)   get2DDataValue()   toYRot()   fromYRot(double)
//   getAxisDirection()     getOpposite()      Plane HORIZONTAL/VERTICAL iterate
//
// Direction ordinals (Direction.java:33-38): DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5.
// Axis ordinals (Direction.java:402-480): X=0,Y=1,Z=2.
// AxisDirection ordinals (Direction.java:546-548): POSITIVE=0,NEGATIVE=1.
// Plane ordinals (Direction.java:576-578): HORIZONTAL=0,VERTICAL=1.
// Floats emitted as raw int bits via Float.floatToRawIntBits; ints decimal.
public class DirectionPlaneParity {
   static final java.io.PrintStream O = System.out;

   static int ord(Direction d) { return d.ordinal(); }

   static final Direction[] ALL = { Direction.DOWN, Direction.UP, Direction.NORTH,
                                    Direction.SOUTH, Direction.WEST, Direction.EAST };

   public static void main(String[] args) throws Exception {
      // No bootstrap needed: Direction is a pure enum with no registry deps.

      // GET2D <ord> <get2DDataValue>   (all 6; verticals DOWN/UP return -1)
      for (Direction d : ALL) {
         O.println("GET2D\t" + ord(d) + "\t" + d.get2DDataValue());
      }

      // TOYROT <ord> <toYRot bits>     (all 6; (data2d & 3) * 90, never throws)
      for (Direction d : ALL) {
         O.println("TOYROT\t" + ord(d) + "\t"
            + String.format("%08x", Float.floatToRawIntBits(d.toYRot())));
      }

      // AXDIR <ord> <getAxisDirection ord> <getAxis ord> <getOpposite ord>
      for (Direction d : ALL) {
         O.println("AXDIR\t" + ord(d) + "\t"
            + d.getAxisDirection().ordinal() + "\t"
            + d.getAxis().ordinal() + "\t"
            + ord(d.getOpposite()));
      }

      // FROM2D <data> <ord>            (from2DDataValue: thorough int battery incl. negatives/edges)
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

      // FROMYROT <yRot bits> <ord>     (fromYRot: dense + wrap + negatives + boundaries)
      double[] yRotCases = {
         -720.0, -540.0, -451.0, -450.0, -449.0, -405.0, -360.0, -315.0, -271.0, -270.0,
         -269.0, -226.0, -225.0, -224.0, -181.0, -180.0, -179.0, -136.0, -135.0, -134.0,
         -91.0, -90.0, -89.0, -46.0, -45.0, -44.0, -1.0, -0.5, 0.0, 0.5, 1.0,
         44.0, 44.9, 45.0, 45.1, 46.0, 89.0, 90.0, 91.0, 134.0, 135.0, 136.0, 179.0,
         180.0, 181.0, 224.0, 225.0, 226.0, 269.0, 270.0, 271.0, 314.0, 315.0, 316.0,
         359.0, 360.0, 361.0, 404.0, 405.0, 449.0, 450.0, 451.0, 540.0, 719.0, 720.0,
         1234.5, -1234.5, 22.5, 67.5, 112.5, 157.5, 202.5, 247.5, 292.5, 337.5,
         0.49999999, -0.49999999, 89.99999, 90.00001
      };
      for (double y : yRotCases) {
         O.println("FROMYROT\t" + String.format("%016x", Double.doubleToRawLongBits(y))
            + "\t" + ord(Direction.fromYRot(y)));
      }

      // PLEN <planeOrd> <length>       (Direction.Plane.length())
      O.println("PLEN\t" + Direction.Plane.HORIZONTAL.ordinal() + "\t"
         + Direction.Plane.HORIZONTAL.length());
      O.println("PLEN\t" + Direction.Plane.VERTICAL.ordinal() + "\t"
         + Direction.Plane.VERTICAL.length());

      // PFACE <planeOrd> <index> <faceOrd>   — Plane iteration order via Iterable<Direction>.
      emitPlaneFaces(Direction.Plane.HORIZONTAL);
      emitPlaneFaces(Direction.Plane.VERTICAL);
   }

   static void emitPlaneFaces(Direction.Plane plane) {
      int i = 0;
      Iterator<Direction> it = plane.iterator();
      while (it.hasNext()) {
         Direction d = it.next();
         O.println("PFACE\t" + plane.ordinal() + "\t" + i + "\t" + ord(d));
         i++;
      }
   }
}
