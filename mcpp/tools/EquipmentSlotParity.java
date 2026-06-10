import net.minecraft.world.entity.EquipmentSlot;

// Ground-truth emitter for net.minecraft.world.entity.EquipmentSlot
// (Minecraft 26.1.2). Emits tab-separated rows consumed by
// EquipmentSlotParityTest.cpp. All ints/booleans are decimal; names are raw.
//
// EquipmentSlot ordinals (EquipmentSlot.java:13-20):
//   MAINHAND=0, OFFHAND=1, FEET=2, LEGS=3, CHEST=4, HEAD=5, BODY=6, SADDLE=7
// EquipmentSlot.Type ordinals (EquipmentSlot.java:95-100):
//   HAND=0, HUMANOID_ARMOR=1, ANIMAL_ARMOR=2, SADDLE=3
//
// We call the REAL public accessors getType/getIndex()/getIndex(base)/getId/
// getFilterBit(offset)/getName/getSerializedName/isArmor/canIncreaseExperience,
// plus EquipmentSlot.BY_ID (the ByIdMap.continuous ZERO mapper) and
// EquipmentSlot.byName(...). No reflection / Unsafe needed: every accessor is
// public.
public class EquipmentSlotParity {
   static final java.io.PrintStream O = System.out;

   static final EquipmentSlot[] ALL = EquipmentSlot.values();

   // A finite battery of `base` / `offset` inputs incl. negative & wrap-ish
   // edges to exercise Java int two's-complement add. (No NaN/etc — these are
   // ints.)
   static final int[] BASES = {
      0, 1, 2, 5, 7, 8, 36, 100, -1, -7, -100,
      Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE - 3
   };
   static final int[] OFFSETS = {
      0, 1, 2, 5, 8, 36, 1000, -1, -8, -1000,
      Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE - 2
   };

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      for (EquipmentSlot s : ALL) {
         int ord = s.ordinal();

         // DATA <ord> <typeOrdinal> <index> <id> <isArmor 0|1> <canXp 0|1> <name> <serName>
         O.println("DATA\t" + ord
            + "\t" + s.getType().ordinal()
            + "\t" + s.getIndex()
            + "\t" + s.getId()
            + "\t" + (s.isArmor() ? 1 : 0)
            + "\t" + (s.canIncreaseExperience() ? 1 : 0)
            + "\t" + s.getName()
            + "\t" + s.getSerializedName());

         // INDEXBASE <ord> <base> <getIndex(base)>
         for (int base : BASES) {
            O.println("INDEXBASE\t" + ord + "\t" + base + "\t" + s.getIndex(base));
         }

         // FILTERBIT <ord> <offset> <getFilterBit(offset)>
         for (int off : OFFSETS) {
            O.println("FILTERBIT\t" + ord + "\t" + off + "\t" + s.getFilterBit(off));
         }
      }

      // BYID <id> <resulting ordinal> — EquipmentSlot.BY_ID is the
      // ByIdMap.continuous(..., ZERO) IntFunction. Probe in-range and
      // out-of-range ids (ZERO fallback -> MAINHAND).
      int[] idProbes = {
         -1000, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1000,
         Integer.MAX_VALUE, Integer.MIN_VALUE
      };
      for (int id : idProbes) {
         EquipmentSlot r = EquipmentSlot.BY_ID.apply(id);
         O.println("BYID\t" + id + "\t" + r.ordinal());
      }

      // BYNAME <name> <resulting ordinal> — valid serialized names only.
      for (EquipmentSlot s : ALL) {
         EquipmentSlot r = EquipmentSlot.byName(s.getSerializedName());
         O.println("BYNAME\t" + s.getSerializedName() + "\t" + r.ordinal());
      }
   }
}
