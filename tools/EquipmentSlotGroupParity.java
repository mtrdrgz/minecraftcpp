// Ground-truth generator for net.minecraft.world.entity.EquipmentSlotGroup parity.
//
// Drives the REAL EquipmentSlotGroup (and the EquipmentSlot it tests) to dump,
// for every enum constant: its name()/ordinal, the network `id` field, the
// getSerializedName() key, the full membership matrix test(EquipmentSlot) over
// EVERY EquipmentSlot, the slots() list (filtered VALUES), the BY_ID lookups
// (ByIdMap.continuous + ZERO out-of-bounds), and bySlot(EquipmentSlot). Also
// dumps each EquipmentSlot's type ordinal/id/name/isArmor so the C++ port's
// predicate inputs are pinned to ground truth too.
//
// The `id` field of both enums is private — read via reflection+setAccessible.
// Everything else is public API.
//
// Output rows (tab-separated). ints/ordinals/bools(0|1) decimal; names/keys raw.
//   SLOT   <ordinal> <name> <typeOrdinal> <typeName> <id> <serializedName> <isArmor>
//   GROUP  <ordinal> <name> <id> <serializedName>
//   TEST   <groupName> <slotName> <testResult0or1>
//   SLOTS  <groupName> <count> <slotName0> <slotName1> ...     (slots() in order)
//   BYID   <queryId> <resultGroupName>
//   BYSLOT <slotName> <resultGroupName>

import java.lang.reflect.Field;

import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.EquipmentSlotGroup;

public class EquipmentSlotGroupParity {
   static final java.io.PrintStream O = System.out;

   static Field groupIdField; // private final int EquipmentSlotGroup.id
   static Field slotIdField;  // private final int EquipmentSlot.id

   static int groupId(EquipmentSlotGroup g) throws Exception {
      return groupIdField.getInt(g);
   }
   static int slotId(EquipmentSlot s) throws Exception {
      return slotIdField.getInt(s);
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      groupIdField = EquipmentSlotGroup.class.getDeclaredField("id");
      groupIdField.setAccessible(true);
      slotIdField = EquipmentSlot.class.getDeclaredField("id");
      slotIdField.setAccessible(true);

      EquipmentSlot[] slots = EquipmentSlot.values();
      EquipmentSlotGroup[] groups = EquipmentSlotGroup.values();

      // ---- EquipmentSlot facts (the predicate inputs) ----
      for (EquipmentSlot s : slots) {
         O.println("SLOT\t" + s.ordinal() + "\t" + s.name()
            + "\t" + s.getType().ordinal() + "\t" + s.getType().name()
            + "\t" + slotId(s) + "\t" + s.getSerializedName()
            + "\t" + (s.isArmor() ? 1 : 0));
      }

      // ---- EquipmentSlotGroup facts ----
      for (EquipmentSlotGroup g : groups) {
         O.println("GROUP\t" + g.ordinal() + "\t" + g.name()
            + "\t" + groupId(g) + "\t" + g.getSerializedName());
      }

      // ---- Full membership matrix: test(slot) for every (group, slot) ----
      for (EquipmentSlotGroup g : groups) {
         for (EquipmentSlot s : slots) {
            O.println("TEST\t" + g.name() + "\t" + s.name()
               + "\t" + (g.test(s) ? 1 : 0));
         }
      }

      // ---- slots() list (the precomputed filtered VALUES, in order) ----
      for (EquipmentSlotGroup g : groups) {
         StringBuilder sb = new StringBuilder("SLOTS");
         sb.append('\t').append(g.name());
         java.util.List<EquipmentSlot> list = g.slots();
         sb.append('\t').append(list.size());
         for (EquipmentSlot s : list) {
            sb.append('\t').append(s.name());
         }
         O.println(sb.toString());
      }

      // ---- BY_ID lookups: in-range ids + out-of-bounds (ZERO strategy) ----
      int[] queryIds = {
         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,   // every valid id
         11, 12, 100, 1000,                  // above range -> zero value
         -1, -2, -100,                       // below range -> zero value
         Integer.MAX_VALUE, Integer.MIN_VALUE
      };
      for (int id : queryIds) {
         EquipmentSlotGroup r = EquipmentSlotGroup.BY_ID.apply(id);
         O.println("BYID\t" + id + "\t" + r.name());
      }

      // ---- bySlot(EquipmentSlot) for every slot ----
      for (EquipmentSlot s : slots) {
         EquipmentSlotGroup r = EquipmentSlotGroup.bySlot(s);
         O.println("BYSLOT\t" + s.name() + "\t" + r.name());
      }
   }
}
