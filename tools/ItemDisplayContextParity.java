// Ground truth for net.minecraft.world.item.ItemDisplayContext (Minecraft 26.1.2).
//
// ItemDisplayContext is a 10-value pure-data enum implementing StringRepresentable:
//   NONE(0, "none")
//   THIRD_PERSON_LEFT_HAND(1, "thirdperson_lefthand")
//   THIRD_PERSON_RIGHT_HAND(2, "thirdperson_righthand")
//   FIRST_PERSON_LEFT_HAND(3, "firstperson_lefthand")
//   FIRST_PERSON_RIGHT_HAND(4, "firstperson_righthand")
//   HEAD(5, "head")
//   GUI(6, "gui")
//   GROUND(7, "ground")
//   FIXED(8, "fixed")
//   ON_SHELF(9, "on_shelf")
//
// We drive the REAL net.minecraft.world.item.ItemDisplayContext enum. The instance
// field `id` is private and stored as a byte, so it is read via reflection +
// setAccessible; `getSerializedName()`, `getId()`, `firstPerson()`, `leftHand()`
// are public and called directly. We also exercise the REAL
// ItemDisplayContext.BY_ID IntFunction (ByIdMap.continuous, ZERO strategy) across
// a battery of int keys.
//
// Row TAGs (tab-separated; ints decimal, strings raw):
//   CTX  <ordinal> <id> <serializedName> <firstPerson> <leftHand>
//        one row per ItemDisplayContext constant. <id> is getId() (byte widened to
//        int); <firstPerson>/<leftHand> are booleans printed as 0/1.
//   BYID <id> <ordinal>
//        BY_ID.apply(id).ordinal() — the resolved ItemDisplayContext for any int
//        key (ZERO strategy: out-of-range -> ordinal 0 / NONE).
//
// The C++ test rebuilds the identical table and compares every field BIT-FOR-BIT.

import java.lang.reflect.Field;
import java.util.function.IntFunction;
import net.minecraft.world.item.ItemDisplayContext;

public class ItemDisplayContextParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Private instance field on ItemDisplayContext (stored as a byte).
        Field idF = ItemDisplayContext.class.getDeclaredField("id");
        idF.setAccessible(true);
        // Also confirm the private `name` field equals getSerializedName().
        Field nameF = ItemDisplayContext.class.getDeclaredField("name");
        nameF.setAccessible(true);

        // ----- one CTX row per enum constant -----
        for (ItemDisplayContext c : ItemDisplayContext.values()) {
            byte id = idF.getByte(c);             // private byte
            int idPublic = c.getId();             // public byte accessor
            if (id != idPublic) {
                throw new IllegalStateException("id mismatch for " + c.name()
                        + ": private=" + id + " public=" + idPublic);
            }
            String serialized = c.getSerializedName();  // public (StringRepresentable)
            String privName = (String) nameF.get(c);
            if (!privName.equals(serialized)) {
                throw new IllegalStateException("name mismatch: " + privName + " vs " + serialized);
            }
            boolean firstPerson = c.firstPerson();
            boolean leftHand = c.leftHand();
            O.println("CTX\t" + c.ordinal() + "\t" + ((int) id) + "\t" + serialized
                      + "\t" + (firstPerson ? 1 : 0) + "\t" + (leftHand ? 1 : 0));
        }

        // ----- BY_ID over a battery of int keys (ZERO strategy) -----
        @SuppressWarnings("unchecked")
        IntFunction<ItemDisplayContext> byId =
                (IntFunction<ItemDisplayContext>) staticField(ItemDisplayContext.class, "BY_ID");

        java.util.LinkedHashSet<Integer> ids = new java.util.LinkedHashSet<>();
        for (int i = -16; i <= 24; i++) ids.add(i);   // straddles the [0,10) window both sides
        ids.add(Integer.MIN_VALUE);
        ids.add(Integer.MIN_VALUE + 1);
        ids.add(Integer.MAX_VALUE);
        ids.add(Integer.MAX_VALUE - 1);
        ids.add(-100000);
        ids.add(100000);
        for (int id : ids) {
            ItemDisplayContext c = byId.apply(id);
            O.println("BYID\t" + id + "\t" + c.ordinal());
        }

        O.flush();
    }

    static Object staticField(Class<?> cls, String name) throws Exception {
        Field f = cls.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(null);
    }
}
