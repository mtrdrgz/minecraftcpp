// Ground truth for the two pure equine enums of Minecraft 26.1.2:
//   net.minecraft.world.entity.animal.equine.Variant   (horse coat color)
//   net.minecraft.world.entity.animal.equine.Markings   (horse markings)
//
// Exercises the REAL classes (no body is reimplemented Java-side):
//   * Variant.values()  / Variant.getId() / Variant.getSerializedName()
//   * Variant.byId(int)   over a wide finite int sweep
//   * Markings.values() / Markings.getId()
//   * Markings.byId(int)  over the same sweep
//
// byId resolves through ByIdMap.continuous(idGetter, values(),
// OutOfBoundsStrategy.WRAP), i.e. sortedValues[Mth.positiveModulo(id, length)]
// with Mth.positiveModulo == Math.floorMod. The sweep deliberately includes
// negatives, Integer.MIN_VALUE / MAX_VALUE and values straddling each modulus so
// the floorMod wrap (not C `%`) is what is being certified.
//
// All methods are public; we call them by reflection only to avoid a hard compile
// dependency on signatures and to read the enum constants generically. Results:
//   - ids are decimal
//   - the resolved enum constant is emitted as its ordinal (decimal)
//   - getSerializedName is emitted base64 (UTF-8 bytes)
//
// Row tags (tab-separated):
//   VID    <id>            <resolvedOrdinal>          Variant.byId(id).ordinal()
//   VGET   <ordinal>       <id> <nameB64>             Variant value: getId + serializedName
//   MID    <id>            <resolvedOrdinal>          Markings.byId(id).ordinal()
//   MGET   <ordinal>       <id>                       Markings value: getId

import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.Base64;
import net.minecraft.world.entity.animal.equine.Markings;
import net.minecraft.world.entity.animal.equine.Variant;

@SuppressWarnings({"deprecation", "unchecked"})
public class EquineVariantsParity {
    static final java.io.PrintStream O = System.out;

    static String b64(String s) {
        return Base64.getEncoder().encodeToString(s.getBytes(StandardCharsets.UTF_8));
    }

    // A finite, deterministic battery of ids that straddles both moduli (5 and 7),
    // covers negatives (the floorMod trap), and the signed-int extremes.
    static int[] sweep() {
        java.util.ArrayList<Integer> ids = new java.util.ArrayList<>();
        // Dense window around zero: covers several full periods either side.
        for (int i = -40; i <= 40; i++) ids.add(i);
        // Larger magnitudes near multiples/just-off-multiples of 5 and 7.
        int[] extra = {
            41, 49, 50, 70, 71, 99, 100, 101, 127, 128, 255, 256, 1000, 1001, 1024,
            -41, -49, -50, -70, -71, -99, -100, -101, -127, -128, -255, -256, -1000, -1024,
            Integer.MAX_VALUE, Integer.MAX_VALUE - 1, Integer.MAX_VALUE - 2,
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1, Integer.MIN_VALUE + 2,
            Integer.MAX_VALUE - 7, Integer.MAX_VALUE - 5,
            Integer.MIN_VALUE + 7, Integer.MIN_VALUE + 5,
            2147483640, -2147483640,
        };
        for (int e : extra) ids.add(e);
        int[] out = new int[ids.size()];
        for (int i = 0; i < out.length; i++) out[i] = ids.get(i);
        return out;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        int[] ids = sweep();

        // ── Variant ───────────────────────────────────────────────────────────
        Method vById = Variant.class.getMethod("byId", int.class);
        Method vGetId = Variant.class.getMethod("getId");
        Method vName = Variant.class.getMethod("getSerializedName");
        Variant[] vValues = (Variant[]) Variant.class.getMethod("values").invoke(null);
        for (Variant v : vValues) {
            int id = (Integer) vGetId.invoke(v);
            String name = (String) vName.invoke(v);
            O.println("VGET\t" + v.ordinal() + "\t" + id + "\t" + b64(name));
        }
        for (int id : ids) {
            Variant v = (Variant) vById.invoke(null, id);
            O.println("VID\t" + id + "\t" + v.ordinal());
        }

        // ── Markings ──────────────────────────────────────────────────────────
        Method mById = Markings.class.getMethod("byId", int.class);
        Method mGetId = Markings.class.getMethod("getId");
        Markings[] mValues = (Markings[]) Markings.class.getMethod("values").invoke(null);
        for (Markings m : mValues) {
            int id = (Integer) mGetId.invoke(m);
            O.println("MGET\t" + m.ordinal() + "\t" + id);
        }
        for (int id : ids) {
            Markings m = (Markings) mById.invoke(null, id);
            O.println("MID\t" + id + "\t" + m.ordinal());
        }
    }
}
