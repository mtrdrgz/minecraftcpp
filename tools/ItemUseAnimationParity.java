// Ground truth for net.minecraft.world.item.ItemUseAnimation (Minecraft 26.1.2).
//
// ItemUseAnimation is a 12-value pure-data enum implementing StringRepresentable:
//   NONE(0, "none")
//   EAT(1, "eat", true)
//   DRINK(2, "drink", true)
//   BLOCK(3, "block")
//   BOW(4, "bow")
//   TRIDENT(5, "trident")
//   CROSSBOW(6, "crossbow")
//   SPYGLASS(7, "spyglass")
//   TOOT_HORN(8, "toot_horn")
//   BRUSH(9, "brush")
//   BUNDLE(10, "bundle")
//   SPEAR(11, "spear", true)
//
// We drive the REAL net.minecraft.world.item.ItemUseAnimation enum. The instance
// fields `id` (int), `name` (String), and `customArmTransform` (boolean) are
// private, so they are read via reflection + setAccessible; `getId()`,
// `getSerializedName()`, `hasCustomArmTransform()` are public and called directly
// (and cross-checked against the private fields). We also exercise the REAL
// ItemUseAnimation.BY_ID IntFunction (ByIdMap.continuous, ZERO strategy) across a
// battery of int keys.
//
// Row TAGs (tab-separated; ints decimal, strings raw):
//   ANIM <ordinal> <id> <serializedName> <customArmTransform>
//        one row per ItemUseAnimation constant. <id> is getId(); <customArmTransform>
//        is hasCustomArmTransform() printed as 0/1.
//   BYID <id> <ordinal>
//        BY_ID.apply(id).ordinal() — the resolved ItemUseAnimation for any int key
//        (ZERO strategy: out-of-range -> ordinal 0 / NONE).
//
// The C++ test rebuilds the identical table and compares every field BIT-FOR-BIT.

import java.lang.reflect.Field;
import java.util.function.IntFunction;
import net.minecraft.world.item.ItemUseAnimation;

public class ItemUseAnimationParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Private instance fields on ItemUseAnimation.
        Field idF = ItemUseAnimation.class.getDeclaredField("id");
        idF.setAccessible(true);
        Field nameF = ItemUseAnimation.class.getDeclaredField("name");
        nameF.setAccessible(true);
        Field catF = ItemUseAnimation.class.getDeclaredField("customArmTransform");
        catF.setAccessible(true);

        // ----- one ANIM row per enum constant -----
        for (ItemUseAnimation c : ItemUseAnimation.values()) {
            int id = idF.getInt(c);               // private int
            int idPublic = c.getId();             // public int accessor
            if (id != idPublic) {
                throw new IllegalStateException("id mismatch for " + c.name()
                        + ": private=" + id + " public=" + idPublic);
            }
            String serialized = c.getSerializedName();  // public (StringRepresentable)
            String privName = (String) nameF.get(c);
            if (!privName.equals(serialized)) {
                throw new IllegalStateException("name mismatch: " + privName + " vs " + serialized);
            }
            boolean cat = catF.getBoolean(c);           // private boolean
            boolean catPublic = c.hasCustomArmTransform();
            if (cat != catPublic) {
                throw new IllegalStateException("customArmTransform mismatch for " + c.name()
                        + ": private=" + cat + " public=" + catPublic);
            }
            O.println("ANIM\t" + c.ordinal() + "\t" + id + "\t" + serialized
                      + "\t" + (cat ? 1 : 0));
        }

        // ----- BY_ID over a battery of int keys (ZERO strategy) -----
        IntFunction<ItemUseAnimation> byId =
                (IntFunction<ItemUseAnimation>) staticField(ItemUseAnimation.class, "BY_ID");

        java.util.LinkedHashSet<Integer> ids = new java.util.LinkedHashSet<>();
        for (int i = -16; i <= 28; i++) ids.add(i);   // straddles the [0,12) window both sides
        ids.add(Integer.MIN_VALUE);
        ids.add(Integer.MIN_VALUE + 1);
        ids.add(Integer.MAX_VALUE);
        ids.add(Integer.MAX_VALUE - 1);
        ids.add(-100000);
        ids.add(100000);
        for (int id : ids) {
            ItemUseAnimation c = byId.apply(id);
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
