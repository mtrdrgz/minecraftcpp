// Ground truth for net.minecraft.world.item.Rarity (Minecraft 26.1.2).
//
// Rarity is a 4-value pure-data enum:
//   COMMON(0, "common", ChatFormatting.WHITE)
//   UNCOMMON(1, "uncommon", ChatFormatting.YELLOW)
//   RARE(2, "rare", ChatFormatting.AQUA)
//   EPIC(3, "epic", ChatFormatting.LIGHT_PURPLE)
//
// We drive the REAL net.minecraft.world.item.Rarity enum and its real
// ChatFormatting color. The instance fields (`id`, `name`) are private, so we
// read them via reflection + setAccessible; `color()` and `getSerializedName()`
// are public and called directly. We also exercise the REAL Rarity.BY_ID
// IntFunction (ByIdMap.continuous, ZERO strategy) across a battery of int keys.
//
// Row TAGs (tab-separated; ints decimal, strings raw):
//   RARITY <ordinal> <id> <serializedName> <colorOrdinal> <colorName> <colorCode> <colorId> <colorValue>
//          one row per Rarity constant. <colorName> is ChatFormatting.name();
//          <colorCode> is getChar() as a decimal char code; <colorValue> is
//          getColor() (never null for these four colors).
//   BYID   <id> <ordinal>
//          BY_ID.apply(id).ordinal() — the resolved Rarity for any int key
//          (ZERO strategy: out-of-range -> ordinal 0 / COMMON).
//
// The C++ test rebuilds the identical table and compares every field BIT-FOR-BIT.

import java.lang.reflect.Field;
import java.util.function.IntFunction;
import net.minecraft.ChatFormatting;
import net.minecraft.world.item.Rarity;

public class RarityParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Private instance fields on Rarity.
        Field idF = Rarity.class.getDeclaredField("id");
        idF.setAccessible(true);
        Field nameF = Rarity.class.getDeclaredField("name");
        nameF.setAccessible(true);

        // ----- one RARITY row per enum constant -----
        for (Rarity r : Rarity.values()) {
            int id = idF.getInt(r);
            String serialized = r.getSerializedName();        // public
            // sanity: the private `name` field must equal getSerializedName()
            String privName = (String) nameF.get(r);
            if (!privName.equals(serialized)) {
                throw new IllegalStateException("name mismatch: " + privName + " vs " + serialized);
            }
            ChatFormatting color = r.color();                 // public accessor
            int colorOrdinal = color.ordinal();
            String colorName = color.name();
            char colorCode = color.getChar();
            int colorId = color.getId();
            Integer colorValue = color.getColor();            // @Nullable Integer
            if (colorValue == null) {
                throw new IllegalStateException("unexpected null color for " + r.name());
            }
            O.println("RARITY\t" + r.ordinal() + "\t" + id + "\t" + serialized
                      + "\t" + colorOrdinal + "\t" + colorName + "\t" + ((int) colorCode)
                      + "\t" + colorId + "\t" + colorValue.intValue());
        }

        // ----- BY_ID over a battery of int keys (ZERO strategy) -----
        @SuppressWarnings("unchecked")
        IntFunction<Rarity> byId = (IntFunction<Rarity>) staticField(Rarity.class, "BY_ID");

        java.util.LinkedHashSet<Integer> ids = new java.util.LinkedHashSet<>();
        for (int i = -16; i <= 16; i++) ids.add(i);   // straddles the [0,4) window both sides
        ids.add(Integer.MIN_VALUE);
        ids.add(Integer.MIN_VALUE + 1);
        ids.add(Integer.MAX_VALUE);
        ids.add(Integer.MAX_VALUE - 1);
        ids.add(-100000);
        ids.add(100000);
        for (int id : ids) {
            Rarity r = byId.apply(id);
            O.println("BYID\t" + id + "\t" + r.ordinal());
        }

        O.flush();
    }

    static Object staticField(Class<?> cls, String name) throws Exception {
        Field f = cls.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(null);
    }
}
