import java.lang.reflect.Field;
import java.util.function.IntFunction;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.EquipmentSlot;

// Ground-truth emitter for net.minecraft.world.InteractionHand (Minecraft 26.1.2).
// Emits tab-separated rows consumed by InteractionHandParityTest.cpp.
// Ints/booleans are decimal; names are raw.
//
// InteractionHand (InteractionHand.java:10-12): MAIN_HAND(0), OFF_HAND(1).
//   - ordinal()/name()   : public Enum surface.
//   - private int id     : read via reflection+setAccessible (InteractionHand.java:16).
//   - BY_ID              : private static final IntFunction<InteractionHand>
//                          (InteractionHand.java:14), read via reflection.
//   - asEquipmentSlot()  : public (InteractionHand.java:22-24); we emit the
//                          resulting EquipmentSlot.ordinal().
public class InteractionHandParity {
   static final java.io.PrintStream O = System.out;

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      InteractionHand[] all = InteractionHand.values();

      // Private `id` field (InteractionHand.java:16).
      Field idField = InteractionHand.class.getDeclaredField("id");
      idField.setAccessible(true);

      // CONST <ordinal> <name> <id> <asEquipmentSlot ordinal>
      for (InteractionHand h : all) {
         int id = idField.getInt(h);
         EquipmentSlot slot = h.asEquipmentSlot();
         O.println("CONST\t" + h.ordinal()
            + "\t" + h.name()
            + "\t" + id
            + "\t" + slot.ordinal());
      }

      // BY_ID — private static final IntFunction<InteractionHand>
      // (ByIdMap.continuous(..., ZERO), InteractionHand.java:14).
      Field byIdField = InteractionHand.class.getDeclaredField("BY_ID");
      byIdField.setAccessible(true);
      @SuppressWarnings("unchecked")
      IntFunction<InteractionHand> byId = (IntFunction<InteractionHand>) byIdField.get(null);

      // BYID <id> <resulting ordinal> — probe in-range and out-of-range ids
      // (ZERO fallback -> MAIN_HAND, ordinal 0).
      int[] idProbes = {
         -1000, -2, -1, 0, 1, 2, 3, 1000,
         Integer.MAX_VALUE, Integer.MIN_VALUE
      };
      for (int id : idProbes) {
         InteractionHand r = byId.apply(id);
         O.println("BYID\t" + id + "\t" + r.ordinal());
      }
   }
}
