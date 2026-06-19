import java.lang.reflect.Field;
import java.util.Set;

import net.minecraft.core.Direction;
import net.minecraft.core.Direction8;
import net.minecraft.core.Vec3i;

// Ground-truth emitter for net.minecraft.core.Direction8 (Minecraft 26.1.2).
// Emits tab-separated rows consumed by Direction8ParityTest.cpp.
//
// Direction8 ordinals (Direction8.java:8-15):
//   NORTH=0, NORTH_EAST=1, EAST=2, SOUTH_EAST=3,
//   SOUTH=4, SOUTH_WEST=5, WEST=6, NORTH_WEST=7
// Direction ordinals (Direction.java:33-38):
//   DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5
//
// All ints/booleans emitted as decimal. We read the private `step` Vec3i field
// via reflection to also emit the (unexposed) Y component, and read the private
// `directions` Set to confirm membership against the public getDirections().
public class Direction8Parity {
   static final java.io.PrintStream O = System.out;

   static final Direction8[] ALL8 = Direction8.values();
   static final Direction[] CARDINALS = {
      Direction.DOWN, Direction.UP, Direction.NORTH,
      Direction.SOUTH, Direction.WEST, Direction.EAST
   };

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      // Reflect the private `step` Vec3i so we can read all 3 components.
      Field stepField = Direction8.class.getDeclaredField("step");
      stepField.setAccessible(true);

      for (Direction8 d8 : ALL8) {
         int ord = d8.ordinal();

         // STEP <d8 ord> <stepX> <stepY> <stepZ>
         //   stepX/stepZ via the public accessors; stepY via reflected Vec3i.
         Vec3i step = (Vec3i) stepField.get(d8);
         O.println("STEP\t" + ord + "\t" + d8.getStepX() + "\t" + step.getY() + "\t" + d8.getStepZ());

         // HAS <d8 ord> <cardinal ord> <0|1 membership in getDirections()>
         //   one row per cardinal Direction (all 6, incl. vertical DOWN/UP which
         //   are never members — exercises the negative path too).
         Set<Direction> dirs = d8.getDirections();
         for (Direction c : CARDINALS) {
            int has = dirs.contains(c) ? 1 : 0;
            O.println("HAS\t" + ord + "\t" + c.ordinal() + "\t" + has);
         }

         // COUNT <d8 ord> <number of contributing cardinals>
         O.println("COUNT\t" + ord + "\t" + d8.getDirections().size());
      }
   }
}
